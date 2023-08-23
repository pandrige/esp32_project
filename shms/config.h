#include <esp_sntp.h>
#include <SPI.h>
#include <SdFat.h>
#include <mpu6500.h>
#include <MQTT.h>
#include <WiFi.h>
#include <queue>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <SimpleKalmanFilter.h>
#define   SUB_TOPIC "command"

TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;
TickType_t xInterval = 1;

bool ntpStatus = false;
const long  gmtOffset_sec = 25200;
const int   daylightOffset_sec = 0;
WiFiClient wifiClient;
MQTTClient mqttClient(16500);
String PUBLISH_TOPIC;
String UPLOAD_TOPIC;
const int PACK_SIZE = 1000 ;
struct __attribute__((packed))pack {
  float v1, v2, v3;
};

struct __attribute__((packed))fullpack {
  uint32_t rawtime;
  char id = 'a';
  uint8_t num = 3;
  char typed[4] = "fff";
  pack buff[PACK_SIZE];
} outpack;

void tobeDeleted();
void initmqttClient();
void samplingData();
void initTime();
void uploadData();
void mqttconnect();
void printLocalTime();
void sendingData();
bool initWiFi();
void mqtt_client_loop();
struct tm timeinfo;
bool start_sampling = false;
std::queue <fullpack*> toSend;

//== == == == == == == == == == == == MPU == == == == == == == == == == == == == == =


bfs::Mpu6500 imu;
bool mpuStatus = false;
void initMpu();

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
String gRange;
String ntpServer;
String qos;
String sensorNum;

// File paths to save input values permanently
const char* sensorNumPath = "/sensorNumPath.txt";
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* brokerPath = "/broker.txt";
const char* usernamePath = "/username.txt";
const char* mqtt_passPath = "/mqtt_pass.txt";
const char* gRangePath = "/gRangePath.txt";
const char* ntpServerPath = "/ntpServerPath.txt";
const char* qosPath = "/qosPatch.txt";
IPAddress localIP;
unsigned long previousMillis = 0;
const long interval = 15000;  // interval to wait for Wi-Fi connection (milliseconds)
