# Devsbot

Devsbot is a library for managing and interacting with Internet of Things (IoT) devices based on ESP32 and ESP8266.

## Installation

To use devsbot, simply download the devsbot library and include it in your Arduino project.

https://github.com/niraltek/devsbot-arduino-library.git

## Usage

Here's an example of how to use devsbot to connect to a remote IoT device:

#include "Devsbot.h"

#define DBOT_AUTH_TOKEN "xxxxxxxxxxxxxxxxxxxx" // define your devsbot authentication token
#define DBOT_CLUSTER_ID  "xxxxxxxxxx" // define your devsbot cluster ID
#define WIFI_SSID "xxxxxxxx"
#define WIFI_PASSWORD "xxxxxxxx"



cls_devsbot iot;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
 dBot.begin(DBOT_CLUSTER_ID, DBOT_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD); // Cluster ID, Auth Token, Wi-Fi SSID, Wi-Fi Password.
}

void loop()
{
  // put your main code here, to run repeatedly:
  dBot.Loop();
}
