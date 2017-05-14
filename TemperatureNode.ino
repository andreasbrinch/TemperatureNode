#include <Homie.h>
#include <WEMOS_SHT3X.h>

extern "C" {
  #include "user_interface.h"
  extern struct rst_info resetInfo;
}

#define FW_NAME "DHTNode"
#define FW_VERSION "1.1.23"

/* Magic sequence for Autodetectable Binary Upload */
const char *__FLAGGED_FW_NAME = "\xbf\x84\xe4\x13\x54" FW_NAME "\x93\x44\x6b\xa7\x75";
const char *__FLAGGED_FW_VERSION = "\x6a\x3f\x3e\x0e\xe1" FW_VERSION "\xb0\x30\x48\xd4\x1a";
/* End of magic sequence for Autodetectable Binary Upload */

SHT3X sht30(0x45);

const unsigned long READ_INTERVAL = 5*60*1000UL; // ms

HomieNode temperatureNode("temperature", "temperature");
HomieNode humidityNode("humidity", "humidity");
HomieNode batteryVoltageNode("voltage", "voltage");
HomieNode debugNode("Debug", "Debug");
HomieNode powerSaving("powerSaving", "powerSaving");

unsigned long lastRead = 0;
unsigned long bootTime = 0;
bool powerSavingOn = false;
unsigned long powerSavingTime = 5*60UL; // s

bool powerSavingHandlerSwitch(const HomieRange& range, const String& value) {
  if (value != "true" && value != "false") return false;
  powerSavingOn = (value == "true");
  powerSaving.setProperty("on").send(value);
  return true;
}

bool powerSavingHandlerTime(const HomieRange& range, const String& value) {
  long newPowerSavingTime = value.toInt();
  if (newPowerSavingTime <= 0) return false;
  powerSavingTime = newPowerSavingTime;
  powerSaving.setProperty("time").send(String(powerSavingTime));
  return true;
}

void readAndSend() {
  sht30.get();
  float temperature = sht30.cTemp;
  float hum = sht30.humidity;
  float voltage = analogRead(A0)/1024.0*5.6;

  if (temperature > 0 && temperature < 50) {
    temperatureNode.setProperty("value").send(String(temperature));
  }
  if (hum >= 0 && hum <= 100) {
    humidityNode.setProperty("value").send(String(hum));
  }
  batteryVoltageNode.setProperty("value").send(String(voltage));  
}

void onHomieEvent(const HomieEvent& event) {
  switch(event.type) {
    case HomieEventType::READY_TO_SLEEP:
      ESP.deepSleep((powerSavingTime*1000UL - (millis() - lastRead))*1000UL); // us
      break;
  }
}

void loopHandler() {
  if (bootTime > 0) {
    debugNode.setProperty("Text").send(String(millis() - bootTime));
    bootTime = 0;
  }
  if ((millis() - lastRead) >= (powerSavingTime*1000UL)) {
    lastRead = millis();
    readAndSend();  
  }
  if (powerSavingOn) {
    Homie.prepareToSleep();
  }
}

void setupHandler() {
  if (resetInfo.reason == REASON_DEEP_SLEEP_AWAKE) {
     debugNode.setProperty("Text").send("Awake from deep sleep.");
  }
  else {
    debugNode.setProperty("Text").send("Awake");
  }
  temperatureNode.setProperty("unit").send("C");
  humidityNode.setProperty("unit").send("%");
  batteryVoltageNode.setProperty("unit").send("V");

  powerSavingOn = false;
  powerSavingTime = 5*60UL; // s
}

void setup() {
  bootTime = millis();
  lastRead = millis() - 1000UL*1000UL*1000UL; // ms
  Homie.disableLedFeedback();

  //Serial.begin(115200);

  pinMode(D0, WAKEUP_PULLUP);

  Homie_setFirmware(FW_NAME, FW_VERSION);
  Homie.onEvent(onHomieEvent);
  Homie.setSetupFunction(setupHandler);
  Homie.setLoopFunction(loopHandler);
  temperatureNode.advertise("unit");
  temperatureNode.advertise("value");
  humidityNode.advertise("unit");
  humidityNode.advertise("value");
  batteryVoltageNode.advertise("unit");
  batteryVoltageNode.advertise("value");
  debugNode.advertise("Text");
  powerSaving.advertise("on").settable(powerSavingHandlerSwitch);
  powerSaving.advertise("time").settable(powerSavingHandlerTime);
  Homie.setup();
}

void loop() {
  Homie.loop();
}

