#include "config.h"


#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

IPAddress local_IP(192, 168, 137, 143);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiUDP udp;
const int port = 4210;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Initializing...");

  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  udp.begin(port);
  Serial.printf("Listening on UDP port %d...\n", port);
}

void loop() {
  // WiFi watchdog
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Disconnected! Reconnecting...");
    WiFi.begin(ssid, password);
    delay(500);
    return;
  }

  int packetSize = udp.parsePacket();

  if (packetSize) {
    char buffer[256];
    int len = udp.read(buffer, 255);

    if (len > 0) {
      buffer[len] = '\0';
      Serial.print("[UDP Received] Data: ");
      Serial.println(buffer);

      Serial2.print(buffer);

      Serial.print("[Serial2 Sent] Forwarding: ");
      Serial.println(buffer);
    }
  }
}