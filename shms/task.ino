void saveData(fullpack *dataToSave) {
  if (sdBeginstatus) {
    unsigned long start_write = millis();
    if (fileInFolder < 0 || fileInFolder > 59) {
      FsFile newFolder;
      folderName = (String)mktime(&timeinfo);
      if (newFolder.mkdir(&dir, folderName.c_str())) {
        fileInFolder = 0;
      }
    }
    String filename = "/DATA/" + folderName + "/" + (String)(dataToSave->rawtime);
    //    Serial.print(filename);
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
}

void mqtt_client_loop(void *param) {
  const TickType_t xFrequency = 1000;
  TickType_t xLastWakeTime = xTaskGetTickCount ();
  for (;;) {
    mqttClient.loop();
    if ( pdTRUE == xTaskDelayUntil( &xLastWakeTime, xFrequency )) {
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
  const TickType_t xFrequency = 500;
  TickType_t xLastWakeTime = xTaskGetTickCount ();
  for (;;) {
    if ( pdTRUE == xTaskDelayUntil( &xLastWakeTime, xFrequency )) {
      if (!toSend.empty()) {
        unsigned long start_write = millis();
        fullpack *dataToSend = toSend.front();
        sprintf(filename, "%u.bin", dataToSend->rawtime);
        Serial.print("Publish : ");
        Serial.print(filename);
        
        if (mqttClient.publish(PUBLISH_TOPIC.c_str(), (char*)dataToSend, sizeof(fullpack), false, qos.toInt())) {
          printf(" succeed \n");
        } else {
          printf(" Failed \n");
          saveData(dataToSend);
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
    char filename[11];
    dir.open("/DATA/", O_READ);
    if (uploadDir.openNext(&dir, O_READ)) {
      uint32_t startTask = millis();
      FsFile myFile;
      if (myFile.openNext(&uploadDir, O_READ)) {
        Serial.print("\n Upload File ");
        myFile.printName(&Serial);
        if (mqttClient.publish(UPLOAD_TOPIC.c_str(), (char*)&myFile, sizeof(fullpack), false, qos.toInt())) {
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
  uint16_t count = 0;
  //  const TickType_t xInterval = 1;
  TickType_t xLastWakeTime = xTaskGetTickCount ();
  SimpleKalmanFilter simpleKalmanFilterX(0.1, 0.1, 0.01);
  SimpleKalmanFilter simpleKalmanFilterY(0.1, 0.1, 0.01);
  SimpleKalmanFilter simpleKalmanFilterZ(0.1, 0.1, 0.01);
  for (;;) {
    if ( pdTRUE == xTaskDelayUntil( &xLastWakeTime, xInterval )) {
      pack* temp = &outpack.buff[count % PACK_SIZE];
      if (imu.Read()) {
        temp->v1 = simpleKalmanFilterX.updateEstimate(imu.accel_x_mps2());
        temp->v2 = simpleKalmanFilterY.updateEstimate(imu.accel_y_mps2());
        temp->v3 = simpleKalmanFilterZ.updateEstimate(imu.accel_z_mps2());
      }
      if (count % PACK_SIZE == PACK_SIZE - 1) {
        if (ntpStatus && start_sampling) {
          getLocalTime(&timeinfo);
          outpack.rawtime = mktime(&timeinfo);
          if (toSend.size() < 10) {
            fullpack* dataq = (fullpack*)malloc(sizeof(fullpack));
            memcpy(dataq, &outpack, sizeof(fullpack));
            toSend.push(dataq);
          }
          Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
        }
        if (Serial.available()) {
          String msg = Serial.readStringUntil('\n');
          if (msg.equals("rst")) {
            Serial.println("Start Reset SSID");
            SPIFFS.remove(ssidPath);
            SPIFFS.remove(passPath);
            ESP.restart();
          }
        }
        if (count == 64999) {
          count = 999;
        }
      }
      count++;
    }
  }
}

void initmqttClient() {
  Serial.print("init mqtt client to broker : ");
  Serial.println(broker);
  mqttClient.begin(broker.c_str(), wifiClient);
  mqttClient.onMessageAdvanced(messageReceived);
  mqttClient.setOptions(1, 1, 500);
  String will_topic = "disconnected";
  mqttClient.setWill(will_topic.c_str(), sensorNum.c_str());
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
  imu.Config(&Wire, bfs::Mpu6500::I2C_ADDR_PRIM);
  if (!imu.Begin()) {
    Serial.println("Error initializing communication with IMU");
    while (1) {}
  }
  if (!imu.ConfigSrd(0)) {
    Serial.println("Error configured SRD");
    while (1) {}
  }
  if (gRange.equals("2G")) {
    imu.ConfigAccelRange(bfs::Mpu6500::ACCEL_RANGE_2G);
  }
  if (gRange.equals("4G")) {
    imu.ConfigAccelRange(bfs::Mpu6500::ACCEL_RANGE_4G);
  }
  if (gRange.equals("8G")) {
    imu.ConfigAccelRange(bfs::Mpu6500::ACCEL_RANGE_8G);
  }
  if (gRange.equals("16G")) {
    imu.ConfigAccelRange(bfs::Mpu6500::ACCEL_RANGE_16G);
  }
  Serial.printf("Current Accel Range = %u \n", imu.accel_range());
}

void messageReceived(MQTTClient * mqttClient, char topic[], char bytes[], int length) {
  String msg = (String)bytes;
  if (msg.equals("start")) {
    Serial.println("Start Sampling");
    start_sampling = true;
  } else if (msg.equals("stop")) {
    Serial.println("Stop Sampling");
    start_sampling = false;
  }
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
  if (mqttClient.connect("", username.c_str(), mqtt_pass.c_str())) {
    printf("\n connect mqtt client \n");
    mqttClient.subscribe(SUB_TOPIC, 2);
    String sub_topic = "to/" + sensorNum;
    mqttClient.subscribe(sub_topic.c_str(), 2);
    mqttClient.publish("connected",sensorNum.c_str(),2);
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
    Serial.println("SPIFFS mounted successfully");
    PUBLISH_TOPIC = "from/" + sensorNum;
    UPLOAD_TOPIC = "from/" + sensorNum + "/upload";
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
        //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    request->send(200, "text/plain", "Done. ESP will restart, connect to your Access Point : " + ssid );
    delay(5000);
    ESP.restart();
  });
  server.begin();
}
