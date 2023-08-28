#include "config.h"

void setup() {
  Serial.begin(115200);
  initSPIFFS();
  initSD();
  initMpu();
  if (!initWiFi()) {
    wifi_AP();
  } else {
    initTime();
    sntp_set_time_sync_notification_cb(cbSyncTime);
    initmqttClient();
    xTaskCreatePinnedToCore(sendingData, "Task1", 10000, NULL, 10, &Task1, 0);
    xTaskCreatePinnedToCore(samplingData, "Task2", 5000, NULL, 10, &Task2, 1);
    xTaskCreatePinnedToCore(mqtt_client_loop, "Task3", 2000, NULL, 9, &Task3, 0);
  }
}

void loop() {
}
