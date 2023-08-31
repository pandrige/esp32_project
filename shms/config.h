#include <esp_sntp.h>
#include <SPI.h>
#include <SdFat.h>
#include <MQTT.h>
#include <WiFi.h>
#include <queue>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Wire.h>
#include <SPIFFS.h>
#include <SimpleKalmanFilter.h>
#include <FastIMU.h>
#define IMU_ADDRESS 0x68    //Change to the address of the IMU



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
String UPLOAD_FOLDER;
int PACK_SIZE;

struct __attribute__((packed))pack {
  float v1, v2, v3;
};
struct __attribute__((packed))header{
  uint64_t esp_id = ESP.getEfuseMac();
  uint32_t foldName;
  uint32_t rawtime;
  char id = 'a';
  uint8_t num = 3;
  char typed[4] = "fff";
};
struct __attribute__((packed))fullpack {
  header hd;
  pack dataku[1000];
}outpack;

void initmqttClient();
void initTime();
bool initWiFi();
void sendingData();
void samplingData();
void uploadData();
void mqttconnect();
void mqtt_client_loop();
void initMpu();
struct tm timeinfo;
bool start_sampling = false;
std::queue <char*> toSend;

//== == == == == == == == == == == == MPU == == == == == == == == == == == == == == =


MPU6050 imu;               //Change to the name of any supported IMU!
calData calib = { 0 };
AccelData accelData;
bool mpuStatus = false;

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
String samplingRate;

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
const char* samplingPath = "/samplingRate.txt";
IPAddress localIP;
uint16_t count = 0;
