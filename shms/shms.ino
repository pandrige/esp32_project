#include "config.h"

Task tasksampling(TASK_MILLISECOND * 2 , TASK_FOREVER, &samplingData, &hpr, false);
Task updateMqttConnection(TASK_SECOND * 30, TASK_FOREVER, &updateConnection, &userScheduler, false);
Task tGarbageCollection(TASK_MILLISECOND * 500, TASK_FOREVER, &tobeDeleted, &userScheduler, false);

void setup() {
  Serial.begin(115200);
  initWifi();
  initmqttClient();
  initSD();
  initMpu();
  userScheduler.setHighPriorityScheduler(&hpr);
  userScheduler.enableAll(true);
  //  tasksampling.enable();
  //  updateMqttConnection.enable();

}

void loop() {
  mqttClient.loop();
  userScheduler.execute();
}
