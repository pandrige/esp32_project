void saveData() {
  fullpack *dataToSave = toSave.front();
  if (sdBeginstatus) {
    unsigned long start_write = millis();
    char filename[32];
    sprintf(filename, "%u.bin", dataToSave->rawtime);
    Serial.print(filename);
    if (!dir.isOpen()) {
      dir.open("/DATA");
    }
    File32 myFile;
    if (myFile.open(&dir, filename, O_WRONLY | O_CREAT )) {
      if (myFile.write((char*)dataToSave, sizeof(fullpack)) > 0) {
        printf(" Write success \n");
        uploadFromSD = true;
      } else {
        printf(" Write failed \n");
      }
      myFile.close();
    } else {
      printf(" Failed to make file \n");
    }
    dir.close();
    printf("\tTime Elapse = %u ms \n", (millis() - start_write));
  }
  toSave.pop();
  delete(dataToSave);
}

void sendingData() {
  if (!toSend.empty()) {
    bool isSended = false;
    fullpack *dataToSend = toSend.front();
    char filename[32];
    sprintf(filename, "%u.bin", dataToSend->rawtime);
    Serial.print(filename);
    if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
      if (mqttClient.publish(PUBLISH_TOPIC, (char*)dataToSend, sizeof(fullpack), true, 1)) {
        printf(" Publish succeed \n");
        isSended = true;
      }
    }
    if (!isSended) {
      printf(" Publish Failed \n");
      fullpack *dataToSave = (fullpack*)malloc(sizeof(fullpack));
      memmove(dataToSave, dataToSend, sizeof(fullpack));
      toSave.push(dataToSave);
      Task *t = new Task(TASK_SECOND * 1, 1, &saveData, &userScheduler, true, OnEnable, OnDisable);
    }
    toSend.pop();
    delete(dataToSend);
  }
}

void uploadData() {
  if (sdBeginstatus) {
    uint32_t startTask = millis();
    if (!dir.isOpen()) {
      dir.open("/DATA");
    } else {
      initSD();
    }
    File32 upload;
    char filename[32];
    if (upload.openNext(&dir, O_READ)) {
      upload.getName(filename, sizeof(filename));
      upload.close();
      Serial.print("\tUploading from SD Card : ");
      Serial.println(filename);
      //      printf("\tUploading %u from SD Card \n",filename);
      if (upload.open(&dir, filename, O_RDONLY)) {
        if (mqttClient.publish(UPLOAD_TOPIC, (char*)&upload, upload.fileSize(), true, 2)) {
          printf("\t\tUpload OK \n");
          upload.close();
          if (dir.remove(filename)) {
            printf("\t\tFile Removed \n");
          }
        } else {
          printf("\t\tUpload Failed \n");
        }
      } else {
        printf("\t\tOpen File Failed \n");
      }
    } else {
      uploadFromSD = false;
    }
    upload.close();
    dir.close();
    printf("\t\tElapse Time : %u ms \n", (millis() - startTask));
  }
}

void samplingData() {
  if (start_sampling) {
    uint32_t t = tasksampling.getRunCounter() - 1;
    pack* temp = &outpack.buff[t % PACK_SIZE];
    temp->v1 = (float)t;//mpu.getAngleX();
    temp->v2 = (float)t;//mpu.getAngleY();
    temp->v3 = (float)t;//mpu.getAngleZ();
    temp->v4 = (float)t;
    if (t % PACK_SIZE == 0) {
      if (ntpStatus) {
        ntpStatus = getLocalTime(&timeinfo);
      }
      //    else {
      //      outpack.rawtime = millis();
      //    }
      outpack.rawtime = mktime(&timeinfo);
      outpack.id = 'i';
      outpack.num = 4;
      strcpy(outpack.typed, "ffff");
    }
    if (t % PACK_SIZE == PACK_SIZE - 1) {
      if (ntpStatus) {
        fullpack* dataq = (fullpack*)malloc(sizeof(fullpack));
        memmove(dataq, &outpack, sizeof(fullpack));
        toSend.push(dataq);
        Task *t = new Task(TASK_MILLISECOND * 1, 1, &sendingData, &userScheduler, true, OnEnable, OnDisable);
      }
    }
  }
}

void initmqttClient() {
  Serial.println("init mqtt client");
  mqttClient.begin(ntpServer, wifiClient);
  mqttClient.onMessageAdvanced(messageReceived);
}
void initWifi() {
  Serial.println("Connecting to WIFI");
  WiFi.begin(STATION_SSID, STATION_PASSWORD);
}
bool initSD() {
  Serial.println("Init Sd Card");
  delay(1000);
  if (sdBeginstatus) {
    return true;
  }
  sdBeginstatus = sd.begin(SD_CONFIG);
  if (sdBeginstatus) {
    Serial.println("SD Card Init Success");
    if (!dir.open("/DATA")) {
      if (sd.mkdir("DATA")) {
        Serial.println("sd.mkdir succes");
        dir = sd.open("/DATA", O_READ);
      } else {
        Serial.println("sd.mkdir failed");
      }
    } else {
      Serial.println("open dir /DATA succes");
    }
  } else {
    Serial.println("SD Card Init Failed");
  }
  return sdBeginstatus;
  delay(1000);
}


bool OnEnable() {
  //  Serial.print("New Task ");
  return  true;
}
void OnDisable() {
  Task *t = &userScheduler.currentTask();
  toDelete.push(t);
  tGarbageCollection.enableIfNot();
  printf("\tFree memory = %u \n", ESP.getFreeHeap());
}
void tobeDeleted() {
  if ( toDelete.empty() ) {
    tGarbageCollection.disable();
    return;
  }
  Task *t = toDelete.front();
  toDelete.pop();
  delete t;
  //  Serial.print("Free memory= "); Serial.println(ESP.getFreeHeap());
}
bool initMpu() {
  if (mpuStatus = mpu.begin()) {
    Serial.println("mpu begin succeed");
  }
  mpu.calcOffsets();
  return mpuStatus;
}
void updateTime() {
  //  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  ntpStatus = getLocalTime(&timeinfo);
}

void messageReceived(MQTTClient * mqttClient, char topic[], char bytes[], int length) {
  Serial.print("Message arrived [");
  Serial.println(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)bytes[i]);
  }
  Serial.println();
}
void updateConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not Connected");
    initWifi();
  } else {
    if (ntpStatus) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      Serial.println("");
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    } else {
      printf("config NTP \n");
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      Task *t = new Task(TASK_SECOND * 0, 1, &updateTime, &userScheduler, true, OnEnable, OnDisable);
    }
    if (!mqttClient.connected()) {
      Task *t = new Task(TASK_SECOND * 0, 1, &mqttconnect, &userScheduler, true, OnEnable, OnDisable);
    } else {
      if (uploadFromSD) {
        Task *t = new Task(TASK_SECOND * 5, 5, &uploadData, &userScheduler, true, OnEnable, OnDisable);
      }
    }
  }
}

void mqttconnect() {
  if (mqttClient.connect("", MQTT_USERNAME, MQTT_PASSWORD)) {
    mqttClient.publish(PUBLISH_TOPIC, "Ready!");
    mqttClient.subscribe(SUB_TOPIC);
  }
}
