#include "FS.h"
#include "sonoff_secrets.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

int relay = 12;
int timezone = -3;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", timezone * 3600);
WiFiClientSecure secureWiFiEspClient;

void callback(char *topic, byte *payload, unsigned int length)
{
    StaticJsonDocument<128> pubSubTopicJsonNotification;
    deserializeJson(pubSubTopicJsonNotification, payload);

    bool hasCarArrived = pubSubTopicJsonNotification["hasCarArrived"];
    if (hasCarArrived)
    {
        pinMode(relay, OUTPUT);
        digitalWrite(relay, HIGH);
        digitalWrite(LED_BUILTIN, LOW);
        delay(3*60*1000);
        pinMode(relay, OUTPUT);
        digitalWrite(relay, LOW);
        digitalWrite(LED_BUILTIN, HIGH);
    }
}

void setup_wifi()
{
    secureWiFiEspClient.setBufferSizes(512, 512);
    WiFi.begin(WIFI_USER, WIFI_PSWD);
    timeClient.begin();
    while (!timeClient.update())
    {
        timeClient.forceUpdate();
    }
    secureWiFiEspClient.setX509Time(timeClient.getEpochTime());
}

PubSubClient client(AWS_ENDPOINT, MQTT_PORT, callback, secureWiFiEspClient);

void reconnect()
{
    while (!client.connected())
    {
        if (client.connect("guapi-ldr-sonoff-1"))
        {
            publishMessageWhenReconnectsToBroker(timeClient.getFormattedDate());
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");

            char buf[256];
            secureWiFiEspClient.getLastSSLError(buf, 256);
            Serial.print("WiFiClientSecure SSL error: ");
            Serial.println(buf);
            delay(5000);
        }
    }
}

void SPIFFSLoading()
{
    if (!SPIFFS.begin())
    {
        Serial.println("Failed to mount file system");
        return;
    }

    Serial.print("Heap: ");
    Serial.println(ESP.getFreeHeap());

    File cert = SPIFFS.open("/cert.der", "r");
    if (!cert)
    {
        Serial.println("Failed to open cert file");
    }
    else
        Serial.println("Success to open cert file");

    delay(1000);

    // Load private key file
    File private_key = SPIFFS.open("/key.der", "r");
    if (!private_key)
    {
        Serial.println("Failed to open private cert file");
    }
    else
        Serial.println("Success to open private cert file");

    delay(1000);

    // Load CA file
    File ca = SPIFFS.open("/rootCA.der", "r");
    if (!ca)
    {
        Serial.println("Failed to open ca ");
    }
    else
        Serial.println("Success to open ca");

    delay(1000);

    if (secureWiFiEspClient.loadCertificate(cert))
        Serial.println("cert loaded");
    else
        Serial.println("cert not loaded");

    if (secureWiFiEspClient.loadPrivateKey(private_key))
        Serial.println("private key loaded");
    else
        Serial.println("private key not loaded");

    if (secureWiFiEspClient.loadCACert(ca))
        Serial.println("ca loaded");
    else
        Serial.println("ca failed");
}

void setup()
{
    Serial.begin(115200);
    setup_wifi();
    delay(1000);
    SPIFFSLoading();
    if (!client.connected())
    {
        reconnect();
    }
}

void publishMessageWhenReconnectsToBroker(String zonedDateTime)
{
    char reconnectMessage[192];
    StaticJsonDocument<192> pubSubJsonSerializable;
    pubSubJsonSerializable["message"] = "Back online - Sonoff - 1 connected";
    pubSubJsonSerializable["sender"] = "guapi-ldr-sonoff-1";
    pubSubJsonSerializable["onlineStatus"] = true;
    pubSubJsonSerializable["timeArrived"] = zonedDateTime;
    serializeJson(pubSubJsonSerializable, reconnectMessage);

    Serial.print("Connected");
    client.publish(AWS_IOT_CORE_STATUS_CHECK_TOPIC, reconnectMessage);
    client.subscribe(AWS_IOT_CORE_TOPIC);
}

void loop()
{
    client.loop();
}
