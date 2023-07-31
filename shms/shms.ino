#include "config.h"

Task tasksampling(TASK_MILLISECOND * 2 , TASK_FOREVER, &samplingData, &hpr, false);
Task updateMqttConnection(TASK_SECOND * 30, TASK_FOREVER, &updateConnection, &userScheduler, false);
Task tGarbageCollection(TASK_MILLISECOND * 500, TASK_FOREVER, &tobeDeleted, &userScheduler, false);
Task updateNTP(TASK_HOUR * 12, TASK_FOREVER, &updateTime, &userScheduler, false);

void setup() {
  Serial.begin(115200);
  initSPIFFS();
  if (!initWiFi()) {
    wifi_AP();
  } else {
    initmqttClient();
    initSD();
    initMpu();
    userScheduler.setHighPriorityScheduler(&hpr);
    userScheduler.enableAll(true);
  }
}

void loop() {
  mqttClient.loop();
  userScheduler.execute();
}
