void saveData() {
  fullpack *dataToSave = toSave.front();
  if (sdBeginstatus) {
    unsigned long start_write = millis();
    if (fileInFolder < 0 || fileInFolder > 99) {
      FsFile newFolder;
      folderName = (String)mktime(&timeinfo);
      if (newFolder.mkdir(&dir, folderName.c_str())) {
        fileInFolder = 0;
      }
    }
    String filename = "/DATA/" + folderName + "/" + (String)(start_write / 1000);
    Serial.print(filename);
    FsFile myFile;
    if (myFile.open(filename.c_str(), O_WRONLY | O_CREAT)) {
      if (myFile.write((char*)dataToSave, sizeof(fullpack)) > 0) {
        printf(" Write success \n");
        uploadFromSD = true;
      } else {
        printf(" Write failed \n");
      }
      myFile.close();
      fileInFolder++;
    } else {
      printf(" Failed to make file \n");
    }
    uint32_t elapse = millis() - start_write;
    printf("\tTime Elapse = %u ms \n", elapse);
  }
  toSave.pop();
  delete(dataToSave);
}

void sendingData() {
  if (!toSend.empty()) {
    unsigned long start_write = millis();
    bool isSended = false;
    fullpack *dataToSend = toSend.front();
    char filename[32];
    sprintf(filename, "%u.bin", dataToSend->rawtime);
    Serial.print(filename);
    if (WiFi.status() == WL_CONNECTED) {
      if (mqttClient.connected()) {
        if (mqttClient.publish(PUBLISH_TOPIC, (char*)dataToSend, sizeof(fullpack), true, qos.toInt())) {
          printf(" Publish succeed \n");
          isSended = true;
        }
      } else {
        Task *r = new Task(TASK_SECOND * 1, 1, &mqttconnect, &userScheduler, true, OnEnable, OnDisable);
      }
    }
    if (!isSended) {
      printf(" Publish Failed \n");
      fullpack *dataToSave = (fullpack*)malloc(sizeof(fullpack)); //2*14
      memmove(dataToSave, dataToSend, sizeof(fullpack));
      toSave.push(dataToSave);
      Task *t = new Task(TASK_SECOND * 1, 1, &saveData, &userScheduler, true, OnEnable, OnDisable);
    } else {
      if (uploadFromSD) {
        Task *t = new Task(TASK_SECOND * 1, 1, &uploadData, &userScheduler, true, OnEnable, OnDisable);
      }
    }
    toSend.pop();
    delete(dataToSend);
    printf("\tTime Elapse = %u ms \n", millis() - start_write);
  }
}

void uploadData() {
  printf("\nUpload From SD Card :\n");
  uint32_t startTask = millis();
  if (sdBeginstatus) {
    FsFile uploadDir;
    char filename[11];
    dir.open("/DATA/", O_READ);
    while (uploadDir.openNext(&dir, O_READ)) {
      uploadDir.getName(filename, sizeof(filename));
      if (!folderName.equals((String)filename)) {
        break;
      }
    }
    if (uploadDir.isOpen()) {
      FsFile myFile;
      if (myFile.openNext(&uploadDir, O_READ)) {
        myFile.printName(&Serial);
        if (mqttClient.connected()) {
          if (mqttClient.publish(UPLOAD_TOPIC, (char*)&myFile, sizeof(fullpack), true, qos.toInt())) {
            printf("\n\tUpload OK \n");
            myFile.getName(filename, sizeof(filename));
            if (uploadDir.remove(filename)) {
              printf("\tFile Removed \n");
            } else {
              printf("\tFile Removed Fail \n");
            }
          } else {
            printf("\n\tUpload Failed \n");
          }
        }
        myFile.close();
      } else {
        if (uploadDir.rmdir()) {
          printf("\n\tDirectory Removed \n");
        }
      }
    } else {
      printf("Directory Empty\n");
      uploadFromSD = false;
    }
  }
  printf("\t\tElapse Time : %u ms \n", (millis() - startTask));
}

void samplingData() {
  if (start_sampling) {
    uint32_t t = tasksampling.getRunCounter() - 1;
    pack* temp = &outpack.buff[t % PACK_SIZE];
    temp->v1 = (float)mpu.getAngleX();
    temp->v2 = (float)mpu.getAngleY();
    temp->v3 = (float)mpu.getAngleZ();
    if (t % PACK_SIZE == 0) {
      if (ntpStatus) {
        ntpStatus = getLocalTime(&timeinfo);
      }
      outpack.rawtime = mktime(&timeinfo);
    }
    if (t % PACK_SIZE == PACK_SIZE - 1) {
      if (Serial.available()) {
        String msg = Serial.readStringUntil('\n');
        if (msg.equals("rst")) {
          Serial.println("Start Reset SSID");
          SPIFFS.remove(ssidPath);
          SPIFFS.remove(passPath);
          ESP.restart();
        }
      }
      if (ntpStatus) {
        fullpack* dataq = (fullpack*)malloc(sizeof(fullpack));
        memcpy(dataq, &outpack, sizeof(fullpack));
        toSend.push(dataq);
        Task *t = new Task(TASK_MILLISECOND * 1, 1, &sendingData, &userScheduler, true, OnEnable, OnDisable);
      } else {
        updateTime();
      }
    }
  }
}

void initmqttClient() {
  Serial.print("init mqtt client to broker : ");
  Serial.println(broker);
  mqttClient.begin(broker.c_str(), wifiClient);
  mqttClient.onMessageAdvanced(messageReceived);
}

bool initSD() {
  Serial.println("Init Sd Card");
  if (sd.begin(SD_CONFIG)) {
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
      FsFile traceDir;
      char nama[11];
      while (traceDir.openNext(&dir, O_READ)) {
        traceDir.getName(nama, sizeof(nama));
        Serial.println(nama);
      }
      if (traceDir.open(&dir, nama, O_READ)) {
        FsFile traceFile;
        while (traceFile.openNext(&traceDir, O_READ)) {
          fileInFolder++;
        }
        folderName = (String)nama;
        printf("File number in Forder : %u \n:", fileInFolder);
        Serial.println("Folder Name : " + folderName);
      }
    }
    sdBeginstatus = true;
  } else {
    Serial.println("SD Card Init Failed");
    sdBeginstatus = true;
  }
  delay(3000);
  return sdBeginstatus;
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
}
bool initMpu() {
  if (mpu.begin()) {
    mpuStatus = true;
    Serial.println("mpu begin succeed");
  } else {
    mpuStatus = false;
    Serial.println("mpu begin failed");
  }
  mpu.calcOffsets();
  return mpuStatus;
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
void updateTime() {
  Serial.println("config NTP ");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer.c_str());
  ntpStatus = getLocalTime(&timeinfo);
}

void updateConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not Connected");
    updateNTP.disable();
  } else {
    if (ntpStatus) {
      updateNTP.enableIfNot();
      Serial.println("");
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    } else {
      Task *t = new Task(TASK_SECOND * 0, 1, &updateTime, &userScheduler, true, OnEnable, OnDisable);
    }
  }
}

void mqttconnect() {
  if (mqttClient.connect("", username.c_str(), mqtt_pass.c_str())) {
    printf("\n connect mqtt client \n");
    mqttClient.subscribe(SUB_TOPIC);
  }
}

void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  } else {
    ssid = readFile(SPIFFS, ssidPath);
    pass = readFile(SPIFFS, passPath);
    broker = readFile(SPIFFS, brokerPath);
    username = readFile (SPIFFS, usernamePath);
    mqtt_pass = readFile (SPIFFS, mqtt_passPath);
    gFactor = readFile (SPIFFS, gFactorPath);
    ntpServer = readFile (SPIFFS, ntpServerPath);
    qos = readFile(SPIFFS, qosPath);
//    Serial.println(ssid);
//    Serial.println(pass);
//    Serial.println(broker);
//    Serial.println(username);
//    Serial.println(mqtt_pass);
//    Serial.println(gFactor);
//    Serial.println(ntpServer);
    Serial.println("SPIFFS mounted successfully");
  }
}

String readFile(fs::FS &fs, const char * path) {
//  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available()) {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- frite failed");
  }
}

bool initWiFi() {
  if (ssid == "" || pass == "") {
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED) {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println(WiFi.localIP());
  return true;
}

void wifi_AP() {
  // Connect to Wi-Fi network with SSID and password
  Serial.println("Setting AP (Access Point)");
  // NULL sets an open Access Point
  WiFi.softAP("SHMS-WIFI-MANAGER", NULL);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/wifimanager.html", "text/html");
  });
  server.serveStatic("/", SPIFFS, "/");
  server.on("/", HTTP_POST, [](AsyncWebServerRequest * request) {
    int params = request->params();
    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        // HTTP POST ssid value
        if (p->name() == "ssid") {
          ssid = p->value().c_str();
          if (!ssid.equals("")) {
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
        }
        // HTTP POST pass value
        if (p->name() == "pass") {
          pass = p->value().c_str();
          if (!pass.equals("")) {
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(SPIFFS, passPath, pass.c_str());
          }
        }
        // HTTP POST ip value
        if (p->name() == "broker") {
          broker = p->value().c_str();
          if (!broker.equals("")) {
            Serial.print("MQTT Broker set to: ");
            Serial.println(broker);
            // Write file to save value
            writeFile(SPIFFS, brokerPath, broker.c_str());
          }
        }
        // HTTP POST gateway value
        if (p->name() == "username") {
          username = p->value().c_str();
          if (!username.equals("")) {
            Serial.print("Username set to: ");
            Serial.println(username);
            // Write file to save value
            writeFile(SPIFFS, usernamePath, username.c_str());
          }
        }
        if (p->name() == "mqtt_pass") {
          mqtt_pass = p->value().c_str();
          if (!mqtt_pass.equals("")) {
            Serial.print("mqtt_pass set to: ");
            Serial.println(mqtt_pass);
            // Write file to save value
            writeFile(SPIFFS, mqtt_passPath, mqtt_pass.c_str());
          }
        }
        if (p->name() == "gFactor") {
          gFactor = p->value().c_str();
          if (!gFactor.equals("")) {
            Serial.print("gFactor set to: ");
            Serial.println(gFactor);
            // Write file to save value
            writeFile(SPIFFS, gFactorPath, gFactor.c_str());
          }
        }
        if (p->name() == "ntp") {
          ntpServer = p->value().c_str();
          if (!ntpServer.equals("")) {
            Serial.print("ntp server set to: ");
            Serial.println(ntpServer);
            // Write file to save value
            writeFile(SPIFFS, ntpServerPath, ntpServer.c_str());
          }
        }
        if (p->name() == "qos") {
          qos = p->value().c_str();
          if (!qos.equals("")) {
            Serial.print("QoS set to: ");
            Serial.println(qos);
            // Write file to save value
            writeFile(SPIFFS, qosPath, qos.c_str());
          }
        }
        //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    request->send(200, "text/plain", "Done. ESP will restart, connect to your Access Point : " + ssid );
    delay(5000);
    ESP.restart();
  });
  server.begin();
}
