#define _TASK_MICRO_RES
#define _TASK_PRIORITY
#define _TASK_WDT_IDS
#define _TASK_TIMECRITICAL
#include <SPI.h>
#include <SdFat.h>
#include <MPU6050_light.h>
#include <TaskScheduler.h>
#include <MQTT.h>
#include <WiFi.h>
#include <queue>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Wire.h>
#include "SPIFFS.h"

#define   PUBLISH_TOPIC     "from/1"
#define   UPLOAD_TOPIC      "from/1/upload"
#define   SUB_TOPIC         "to/1"

bool ntpStatus = false;
const long  gmtOffset_sec = 25200;
const int   daylightOffset_sec = 0;
Scheduler userScheduler, hpr;
WiFiClient wifiClient;
MQTTClient mqttClient(16500);

const int PACK_SIZE = 1000 ;
struct __attribute__((packed))pack {
  float v1, v2, v3;
};

struct __attribute__((packed))fullpack {
  uint32_t rawtime;
  char id = 'i';
  uint8_t num = 3;
  char typed[5] = "fff";
  pack buff[PACK_SIZE];
} outpack;

//void messageReceived(MQTTClient *mqttClient, char topic[], char bytes[], int length);
void tobeDeleted();
void initmqttClient();
void samplingData();
void updateConnection();
void updateTime();
void uploadData();
void mqttconnect();
bool OnEnable();
void OnDisable();
void printLocalTime();
void sendingData();
bool initWiFi();
struct tm timeinfo;
bool start_sampling = true;
std::queue <Task*> toDelete;
std::queue <fullpack*> toSend;
std::queue <fullpack*> toSave;

//== == == == == == == == == == == == MPU == == == == == == == == == == == == == == =

MPU6050 mpu(Wire);
bool mpuStatus = false;
bool initMpu();

//== == == == == == == == == == == = SDCARD == == == == == == == == == == == == =
const uint8_t SD_CS_PIN = SS;
const uint8_t SOFT_MISO_PIN = MISO;
const uint8_t SOFT_MOSI_PIN = MOSI;
const uint8_t SOFT_SCK_PIN = SCK;
SoftSpiDriver<SOFT_MISO_PIN, SOFT_MOSI_PIN, SOFT_SCK_PIN> softSpi;
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(50), &softSpi)
SdFat sd;
FsFile dir;
bool sdBeginstatus = false;
bool initSD();
bool uploadFromSD = true;
String folderName;
int fileInFolder = -1;
/*================================ WIFI MANAGER ==================================*/
AsyncWebServer server(80);

//Variables to save values from HTML form
String ssid;
String pass;
String broker;
String username;
String mqtt_pass;
String gFactor;
String ntpServer;
String qos;

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* brokerPath = "/broker.txt";
const char* usernamePath = "/username.txt";
const char* mqtt_passPath = "/mqtt_pass.txt";
const char* gFactorPath = "/gFactorPath.txt";
const char* ntpServerPath = "/ntpServerPath.txt";
const char* qosPath = "/qosPatch.txt";
IPAddress localIP;
unsigned long previousMillis = 0;
const long interval = 15000;  // interval to wait for Wi-Fi connection (milliseconds)
