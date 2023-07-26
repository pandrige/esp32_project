//#include "sdios.h"
//#include <EEPROM.h>
//#define _TASK_SELF_DESTRUCT
//#define _TASK_SLEEP_ON_IDLE_RUN
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

#define   STATION_SSID      "shms"
#define   STATION_PASSWORD  "rumah28!!"
#define   PUBLISH_TOPIC     "from/1"
#define   UPLOAD_TOPIC      "from/1/upload"
#define   SUB_TOPIC         "to/1"
#define   MQTT_USERNAME     "esp32"
#define   MQTT_PASSWORD     "rumah28!!"

IPAddress myIP(0, 0, 0, 0);
//IPAddress mqttBroker(192, 168, 4, 1);
//const char* ntpServer = "pool.ntp.org";
const char* ntpServer = "192.168.4.1";
const long  gmtOffset_sec = 25200;
const int   daylightOffset_sec = 0;
bool ntpStatus;
Scheduler userScheduler,hpr;
WiFiClient wifiClient;
MQTTClient mqttClient(16100);

const int PACK_SIZE = 500 ;
struct __attribute__((packed))pack {
  float v1, v2, v3, v4;
};

struct __attribute__((packed))fullpack {
  uint32_t rawtime;
  char id;
  uint8_t num;
  char typed[5];
  pack buff[PACK_SIZE];
} outpack;

void messageReceived(MQTTClient *mqttClient, char topic[], char bytes[], int length);
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
void initWifi();
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
SdFat32 sd;
File32 dir;
bool sdBeginstatus = false;
bool initSD();
bool uploadFromSD = true;
