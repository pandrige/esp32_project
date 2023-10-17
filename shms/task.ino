void saveData(int size, String name) {
  char* dataToSave = toSend.front();
  if (sdBeginstatus) {
    unsigned long start_write = millis();
    if (fileInFolder < 0 || fileInFolder > 59) {
      FsFile newFolder;
      folderName = (String)mktime(&timeinfo);
      if (newFolder.mkdir(&dir, folderName.c_str())) {
        fileInFolder = 0;
      }
    }
    String filename = "/DATA/" + folderName + "/" + name;
    //    Serial.print(filename);
    FsFile myFile;
    if (myFile.open(filename.c_str(), O_WRONLY | O_CREAT)) {
      if (myFile.write((char*)dataToSave, size) > 0) {
        printf(" Write success \n");
        uploadFromSD = true;
        fileInFolder++;
      } else {
        printf(" Write failed \n");
      }
      myFile.close();
    } else {
      printf(" Failed to make file \n");
    }
    uint32_t elapse = millis() - start_write;
    printf("\tTime Elapse = %u ms \n", elapse);
  }
}

void mqtt_client_loop(void *param) {
  const TickType_t xInterval = 2000;
  TickType_t xLastWakeTime = xTaskGetTickCount ();
  Serial.println("Start Task mqtt client loop");
  for (;;) {
    mqttClient.loop();
    if ( pdTRUE == xTaskDelayUntil( &xLastWakeTime, xInterval )) {
      if (WiFi.status() == WL_CONNECTED) {
        if (!mqttClient.connected()) {
          mqttconnect();
        }
      }
    }
  }
}

void sendingData(void *param) {
  char filename[32];
  printf("Start Task sending data on Core %u \n", xPortGetCoreID());
  const TickType_t xInterval = 300;
  TickType_t xLastWakeTime = xTaskGetTickCount ();
  for (;;) {
    if ( pdTRUE == xTaskDelayUntil( &xLastWakeTime, xInterval )) {
      if (!toSend.empty()) {
        unsigned long start_write = millis();
        char* dataToSend = toSend.front();
        header tempHd;
        memcpy(&tempHd, dataToSend, sizeof(header));
        sprintf(filename, "%u.bin", tempHd.rawtime);
        Serial.print("Publish : ");
        Serial.print(filename);
        int sizeFile = sizeof(header) + (sizeof(pack) * PACK_SIZE);
        if (mqttClient.publish(PUBLISH_TOPIC.c_str(), dataToSend, sizeFile, false, qos.toInt())) {
          printf(" succeed \n");
        } else {
          printf(" Failed \n");
          saveData(sizeFile, (String)filename);
        }
        toSend.pop();
        delete(dataToSend);
        printf("\tTime Elapse = %u ms \n", millis() - start_write);
        printf("\tFree memory = %u \n", ESP.getFreeHeap());
      }
      if (uploadFromSD) {
        if (mqttClient.connected()) {
          uploadData();
        }
      }
    }
  }
}

void uploadData() {
  if (sdBeginstatus) {
    FsFile uploadDir;
    char filename[20];
    dir.open("/DATA/", O_READ);
    if (uploadDir.openNext(&dir, O_READ)) {
      uint32_t startTask = millis();
      FsFile myFile;
      if (myFile.openNext(&uploadDir, O_READ)) {
        Serial.print("\n Upload File ");
        char temp[myFile.fileSize()];
        myFile.printName(&Serial);
        myFile.read(&temp, myFile.fileSize());
        if (mqttClient.publish(UPLOAD_TOPIC.c_str(), (char*)&temp, sizeof(temp), false, qos.toInt())) {
          printf("\tSucceed \n");
          myFile.getName(filename, sizeof(filename));
          if (uploadDir.remove(filename)) {
            printf("\tFile Removed \n");
          } else {
            printf("\tFile Removed Fail \n");
          }
        } else {
          printf("\tFailed \n");
        }
        myFile.close();
        printf("\tElapse Time : %u ms \n", (millis() - startTask));
      } else {
        uploadDir.getName(filename, sizeof(filename));
        if (!folderName.equals((String)filename)) {
          if (uploadDir.rmdir()) {
            printf("\tDirectory Removed \n");
          }
        } else {
          uploadFromSD = false;
        }
      }
    } else {
      printf("\tDirectory Empty\n");
      uploadFromSD = false;
    }
  }
}

void samplingData(void* param) {
  printf("Start Task sampling data on Core %u \n", xPortGetCoreID());
  TickType_t xLastWakeTime = xTaskGetTickCount ();
  PACK_SIZE = samplingRate.toInt();
  xInterval = 1000 / PACK_SIZE;
  for (;;) {
    if ( pdTRUE == xTaskDelayUntil( &xLastWakeTime, xInterval )) {
      if (is_ssid_reset()) {
        ESP.restart();
      }
      if (start_sampling) {
        if (ntpStatus) {
          getLocalTime(&timeinfo);
          outpack.hd.rawtime = mktime(&timeinfo);
          if (outpack.hd.rawtime >= msgIn.timeStart) {
            imu.update();
            imu.getAccel(&accelData);
            pack* temp = &outpack.dataku[count % PACK_SIZE];
            temp->v1 = accelData.accelX;
            temp->v2 = accelData.accelY;
            temp->v3 = accelData.accelZ;
            if (count % PACK_SIZE == PACK_SIZE - 1) {
              if (toSend.size() < 10) {
                char* dataq = (char*)malloc(sizeof(header) + (sizeof(pack) * PACK_SIZE));
                memcpy(dataq, &outpack, (sizeof(header) + (sizeof(pack)*PACK_SIZE)));
                toSend.push(dataq);
              }
              //          Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
              if (count == 64999) {
                count = 999;
              }
            }
            count++;
          }
          //Set AutoStop 
          if (msgIn.delayStop > 0) {
            if (outpack.hd.rawtime>=(msgIn.timeStart+msgIn.delayStop)) {
              start_sampling=false;
              Serial.println("Stop Sampling");
            }
          }
        }
      }
    }
  }
}

bool is_ssid_reset() {
  bool rst = false;
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    if (msg.equals("rst")) {
      Serial.println("Start Reset SSID");
      SPIFFS.remove(ssidPath);
      SPIFFS.remove(passPath);
      rst = true;
    }
  }
  return rst;
}

void initmqttClient() {
  Serial.print("init mqtt client to broker : ");
  Serial.println(broker);
  mqttClient.begin(broker.c_str(), wifiClient);
  mqttClient.onMessageAdvanced(messageReceived);
  mqttClient.setOptions(1, 1, 500);
  String will_topic = "connection/" + sensorNum;
  //  String msg = "0";
  Status disConnect;
  disConnect.msg = '0';
  samplingRate.toCharArray(disConnect.sampling_R, 5);
  gRange.toCharArray(disConnect.gravity_R, 3);
  mqttClient.setWill(will_topic.c_str(), (char*)&disConnect, true, 2);
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
    }
    sdBeginstatus = true;
  } else {
    Serial.println("SD Card Init Failed");
    sdBeginstatus = true;
  }
  delay(3000);
  return sdBeginstatus;
}

void initMpu() {
  Wire.begin();
  Wire.setClock(400000);
  int err = imu.init(calib, IMU_ADDRESS);
  if (err != 0) {
    Serial.print("Error initializing IMU: ");
    Serial.println(err);
    while (true) {}
  }
  Serial.println("Keep IMU level For Calibrating..");
  delay(5000);
  imu.calibrateAccelGyro(&calib);
  Serial.println("Calibration done!");

  if (gRange.equals("2G")) {
    err = imu.setAccelRange(2);
  }
  else if (gRange.equals("4G")) {
    err = imu.setAccelRange(4);
  }
  else if (gRange.equals("8G")) {
    err = imu.setAccelRange(8);
  }
  else if (gRange.equals("16G")) {
    err = imu.setAccelRange(16);
  }
  if (err != 0) {
    Serial.print("Error Setting Acceleration range: ");
    Serial.println(err);
    while (true) {
      ;
    }
  }
}

void messageReceived(MQTTClient * mqttClient, char topic[], char bytes[], int length) {
  String temp = (String)bytes;
  memcpy(&msgIn, &temp, length);
  Serial.println("Message Coming");
  Serial.println(msgIn.head);
  if (msgIn.head == 'A') {
    Serial.println("Start Sampling");
    outpack.hd.foldName = msgIn.timeStart;
    count = 0;
    start_sampling = true;
  } else if (msgIn.head == 'Z') {
    Serial.println("Stop Sampling");
    count = 0;
    start_sampling = false;
  }

  /*
    String msg = (String)bytes;
    if (msg.startsWith("start")) {
    Serial.println("Start Sampling");
    UPLOAD_FOLDER = msg.substring(5);
    count = 0;
    start_sampling = true;
    } else if (msg.equals("stop")) {
    Serial.println("Stop Sampling");
    start_sampling = false;
    UPLOAD_FOLDER = "";
    } if (start_sampling == false) {
    if (msg.equals("50")) {
      Serial.println("set sampling rate = 50");
      PACK_SIZE = 50;
      xInterval = 1000 / PACK_SIZE;
      count = 0;
    } else if (msg.equals("500")) {
      Serial.println("set sampling rate = 500");
      PACK_SIZE = 500;
      xInterval = 1000 / PACK_SIZE;
      count = 0;
    } else if (msg.equals("200")) {
      Serial.println("set sampling rate = 200");
      PACK_SIZE = 200;
      xInterval = 1000 / PACK_SIZE;
      count = 0;
    } else if (msg.equals("100")) {
      Serial.println("set sampling rate = 100");
      PACK_SIZE = 100;
      xInterval = 1000 / PACK_SIZE;
      count = 0;
    } else if (msg.equals("1000")) {
      PACK_SIZE = 1000;
      xInterval = 1000 / PACK_SIZE;
      count = 0;
    }
    }
  */
}
void initTime() {
  Serial.println("config NTP ");
  sntp_set_sync_interval(12 * 60 * 60 * 1000UL); // 12 hours
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer.c_str());
}

void cbSyncTime(struct timeval * tv)  { // callback function to show when NTP was synchronized
  Serial.println(F("NTP time synched"));
  ntpStatus = true;
}

void mqttconnect() {
  Serial.println("connecting to mqtt broker");
  if (mqttClient.connect("", username.c_str(), mqtt_pass.c_str())) {
    printf("\n connect mqtt client \n");
    mqttClient.subscribe("command", 2);
    String sub_topic = "to/" + sensorNum;
    mqttClient.subscribe(sub_topic.c_str(), 2);
    String will_topic = "connection/" + sensorNum;
    Status payload;
    payload.msg = '1';
    samplingRate.toCharArray(payload.sampling_R, 5);
    Serial.println(payload.sampling_R);
    gRange.toCharArray(payload.gravity_R, 3);
    Serial.println(payload.gravity_R);
    //    String msg = "1";
    mqttClient.publish(will_topic.c_str(), (char*)&payload, sizeof(payload), true, 2);
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
    gRange = readFile (SPIFFS, gRangePath);
    ntpServer = readFile (SPIFFS, ntpServerPath);
    qos = readFile(SPIFFS, qosPath);
    sensorNum = readFile(SPIFFS, sensorNumPath);
    samplingRate = readFile(SPIFFS, samplingPath);
    Serial.println("SPIFFS mounted successfully");
    PUBLISH_TOPIC = "from/" + sensorNum;
    UPLOAD_TOPIC = "upload/" + sensorNum;
  }
}

String readFile(fs::FS & fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);
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
void writeFile(fs::FS & fs, const char * path, const char * message) {
  //  Serial.printf("Writing file: %s\r\n", path);
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

  while (WiFi.status() != WL_CONNECTED) {
    if (is_ssid_reset()) {
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
  String AP = "SHMS-" + String(outpack.hd.esp_id, HEX);
  WiFi.softAP(AP.c_str(), NULL);

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
        if (p->name() == "gRange") {
          gRange = p->value().c_str();
          if (!gRange.equals("")) {
            Serial.print("G Range set to: ");
            Serial.println(gRange);
            // Write file to save value
            writeFile(SPIFFS, gRangePath, gRange.c_str());
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
        if (p->name() == "sensorNum") {
          sensorNum = p->value().c_str();
          if (!sensorNum.equals("")) {
            Serial.print("Sensor Number set to: ");
            Serial.println(sensorNum);
            // Write file to save value
            writeFile(SPIFFS, sensorNumPath, sensorNum.c_str());
          }
        }
        if (p->name() == "samplingRate") {
          samplingRate = p->value().c_str();
          if (!samplingRate.equals("")) {
            Serial.print("Sampling Rate set to: ");
            Serial.println(samplingRate);
            // Write file to save value
            writeFile(SPIFFS, samplingPath, samplingRate.c_str());
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
