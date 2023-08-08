#include "config.h"

Task tasksampling(TASK_MILLISECOND * 1 , TASK_FOREVER, &samplingData, &hpr, false);
//Task updateMqttConnection(TASK_SECOND * 30, TASK_FOREVER, &updateConnection, &userScheduler, false);
Task tGarbageCollection(TASK_MILLISECOND * 10, TASK_FOREVER, &tobeDeleted, &userScheduler, false);
Task updateNTP(TASK_HOUR * 12, TASK_FOREVER, &updateTime, &userScheduler, false);

void setup() {
  Serial.begin(115200);
  initSPIFFS();
  initSD();
  if (!initWiFi()) {
    wifi_AP();
  } else {
    initmqttClient();
    initMpu();
    userScheduler.setHighPriorityScheduler(&hpr);
    userScheduler.enableAll(true);
  }
}

void loop() {
  mqttClient.loop();
  userScheduler.execute();
}
