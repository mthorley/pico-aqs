#include "HAIntegration.h"
#include "Credentials.h"

#include <ArduinoHA.h>
#include <WiFi.h>
#include "OXRS_LOG.h"

//Adapted via:
//  https://github.com/dawidchyrzynski/arduino-home-assistant/blob/main/examples/nano33iot/nano33iot.ino

#define LED_PIN     LED_BUILTIN

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);
HASwitch led("led");
HANumber publishFrequency("publishfreq");

static const char* _LOG_PREFIX = "[HAIntegration] ";

void HAIntegration::configure() {

    //Prepare LED:

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);    
    
    //Set device ID as MAC address

    byte mac[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(mac);
    device.setUniqueId(mac, sizeof(mac));

    //Device metadata:

    device.setName("Pico AQS");
    device.setSoftwareVersion("0.1");

    // handle switch state
    led.onCommand(switchHandler);
    led.setName("Board LED"); // optional

    publishFrequency.setName("Publish frequency");
    publishFrequency.setCurrentState((uint32_t)30);
    publishFrequency.onCommand(pubfreqHandler);
    publishFrequency.setIcon("mdi:cog-refresh");
    publishFrequency.setMax(86400);
    publishFrequency.setMin(0);
    publishFrequency.setUnitOfMeasurement("secs");

    
    if (mqtt.begin(MQTT_BROKER, MQTT_LOGIN, MQTT_PASSWORD) == true) {
        LOGF_INFO("Connected to MQTT broker: %s", MQTT_BROKER.toString().c_str());
    } else {
        Serial.print("Could not connect to MQTT broker");
    }
}

void HAIntegration::switchHandler(bool state, HASwitch* sender) {
    digitalWrite(LED_PIN, (state ? HIGH : LOW));
    sender->setState(state);  // report state back to Home Assistant
}

void HAIntegration::pubfreqHandler(HANumeric value, HANumber* sender) {
    Serial.println(value.toInt32());
}

void HAIntegration::loop() {
    mqtt.loop();
}
