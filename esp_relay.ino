#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266mDNS.h>
#include "FS.h"
#include <ESPAsyncWebServer.h>
#define ESPALEXA_ASYNC
#include <Espalexa.h>
#include "wifi_config.h"
#include <NTPClient.h>

#define HOUR_BLINK_MIN 8
#define HOUR_BLINK_MAX 22

byte SWITCH_ON[] = {0xA0, 0x01, 0x01, 0xA2};
byte SWITCH_OFF[] = {0xA0, 0x01, 0x00, 0xA1};

bool isSwitchOn = false;
void turnSwitchOn(){
  isSwitchOn = true;
  Serial.write(SWITCH_ON, sizeof(SWITCH_ON));
  Serial.println("Switch on");
}

void turnSwitchOff(){
  isSwitchOn = false;
  Serial.write(SWITCH_OFF, sizeof(SWITCH_OFF));
  Serial.println("Switch off");
}

void sendSwitchStatus(AsyncWebServerRequest *request){
  char jsonMsg[100];
  sprintf(jsonMsg, "{\"switchOn\": %s}\n", (isSwitchOn? "true":"false"));
  Serial.println(jsonMsg);
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", jsonMsg);
  response->addHeader("Access-Control-Allow-Origin","*");
  request->send(response);
}

AsyncWebServer server(80);
Espalexa espalexa;

void setupWebServerWithAlexa(){

  server.onNotFound([](AsyncWebServerRequest *request){
      if (!espalexa.handleAlexaApiCall(request)) {
        request->send(404, "text/plain", "Not found\n");
      }
  });

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.on("/switchOn", HTTP_GET, [](AsyncWebServerRequest *request){
    turnSwitchOn();
    sendSwitchStatus(request);
  });

  server.on("/switchOff", HTTP_GET, [](AsyncWebServerRequest *request){
    turnSwitchOff();
    sendSwitchStatus(request);
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    sendSwitchStatus(request);
  });

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  espalexa.addDevice("Office lamp", [](uint8_t brightness,  uint32_t rgb) {
    if (brightness < 128){
      turnSwitchOff();
    } else{
      turnSwitchOn();
    }
  });
  espalexa.begin(&server);

}

void blinkBuiltinLed(unsigned int timeToWait){
  digitalWrite(LED_BUILTIN, LOW);
  delay(timeToWait);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(timeToWait);
}

wl_status_t connectToWifiBlinking(){
  wl_status_t current_status = WiFi.status();
  for (byte i=0; i<50; i++){
      if (current_status == WL_CONNECTED) {
        return current_status;
      }
      blinkBuiltinLed(100);
      current_status = WiFi.status();
  }
  return current_status;
}

void connectToWifi(){
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.print("Connecting to WiFi... ");
  while(1){
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    if (connectToWifiBlinking() == WL_CONNECTED){
      break;
    }
    Serial.println(" failed.");
    Serial.print("Retrying... ");
  }
  Serial.println(" connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 0;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup() {
  Serial.begin(9600);

  char* hostString = "office-lamp";
  WiFi.hostname(hostString);
  connectToWifi();

  timeClient.begin();
  timeClient.update();

  SPIFFS.begin();

  setupWebServerWithAlexa();

  MDNS.begin(hostString, WiFi.localIP());
  MDNS.addService("http", "tcp", 80);
}

unsigned int tick = 0;
void loop() {
  int currentHour = timeClient.getHours();
  bool shouldBlink = HOUR_BLINK_MIN < currentHour && currentHour < HOUR_BLINK_MAX;
  if (shouldBlink){
    if (++tick % 5000==0){
      blinkBuiltinLed(50);
      tick = 0;
    }
  }
  MDNS.update();
  espalexa.loop();
  delay(1);
}
