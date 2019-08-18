#include <FS.h>          // this needs to be first, or it all crashes and burns...
/*
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2019 Sensnology AB
 * Full contributor list: https://github.com/mysensors/MySensors/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * REVISION HISTORY
 * Version 1.0 - tekka
 *
 * DESCRIPTION
 * The ESP32 gateway sends data received from sensors to the WiFi link.
 * The gateway also accepts input on ethernet interface, which is then sent out to the radio network.
 *
 * Make sure to fill in your ssid and WiFi password below.
 */

// Enable debug prints to serial monitor
#define MY_DEBUG

// Enables and select radio type (if attached)
#define MY_RADIO_RF24
//#define MY_RADIO_RFM69
//#define MY_RADIO_RFM95

#define MY_GATEWAY_MQTT_CLIENT
#define MY_GATEWAY_ESP32

#define MY_WIFI_SSID ""
#define MY_WIFI_PASSWORD ""

// Set this node's subscribe and publish topic prefix
#define MY_MQTT_PUBLISH_TOPIC_PREFIX "mygateway1-out"
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX "mygateway1-in"

// Set MQTT client id
#define MY_MQTT_CLIENT_ID "mysensors-1"

// Enable these if your MQTT broker requires usenrame/password
//#define MY_MQTT_USER
//#define MY_MQTT_PASSWORD

// Set the hostname for the WiFi Client. This is the hostname
// passed to the DHCP server if not static.
#define MY_HOSTNAME "MySensor_ESP32_GW"

// Enable MY_IP_ADDRESS here if you want a static ip address (no DHCP)
//#define MY_IP_ADDRESS 192,168,178,87

// If using static ip you can define Gateway and Subnet address as well
//#define MY_IP_GATEWAY_ADDRESS 192,168,178,1
//#define MY_IP_SUBNET_ADDRESS 255,255,255,0

// MQTT SERVER IP ADDRESS
IPAddress MY_MQTT_SERVER_IPADDRESS;

// MQTT broker ip address.
#define MY_CONTROLLER_IP_ADDRESS MY_MQTT_SERVER_IPADDRESS[0], MY_MQTT_SERVER_IPADDRESS[1], MY_MQTT_SERVER_IPADDRESS[2], MY_MQTT_SERVER_IPADDRESS[3]

// The MQTT broker port to to open
// #define MY_PORT 1883

// WIFI 热点名字
#define WIFI_HOT_SPOT_SSID "MySensor_ESP32_GW"
// WIFI 热点密码
#define WIFI_HOT_SPOT_PASSWORD "mysensor"

#include <Arduino.h>
#include <MySensors.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson

#ifdef ESP32
  #include <SPIFFS.h>
#endif

// 取消 MySensors 中 Mqtt 的预编译定义
#undef MY_MQTT_USER
#undef MY_PORT
#undef MY_MQTT_PASSWORD

// 重新定义 MySensor 的 Mqtt 参数
uint16_t MY_PORT = 1883;
char *MY_MQTT_USER;
char *MY_MQTT_PASSWORD;

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_user[40];
char mqtt_password[40];
char mqtt_port[6]  = "1883";

// WiFiManage
WiFiManager wm;

// 重置按钮
int buttonState;

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//按 GPIO 重置配置
void Chack_Button_Reset(){
  buttonState = digitalRead(0);
  if (buttonState == LOW) {
    Serial.println("Reset Settings and Reboot.");
    delay(1000);
    wm.resetSettings();
    ESP.restart();
  }
}

void setupSpiffs(){
  //clean FS, for testing
  // SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin(true)) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          strcpy(mqtt_port, json["mqtt_port"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
    Serial.println("Format FS....");
    SPIFFS.format();
    ESP.restart();
  }
}

// 转换 mqtt 数据对接 MySensor
bool MY_Mqtt_Config(){
  // init MY_MQTT_SERVER_IPADDRESS
  if (!MY_MQTT_SERVER_IPADDRESS.fromString(mqtt_server))
  {
    if(!WiFi.hostByName(mqtt_server,MY_MQTT_SERVER_IPADDRESS)){
      Serial.print("The MQTT server address resolution error.");
      Chack_Button_Reset();
      return false;
    }
  }
  // init MY_PORT
  String(mqtt_port) == "" ? MY_PORT=1883 : MY_PORT=String(mqtt_port).toInt();
  // init mqtt_user
  String(mqtt_user) == "" ? MY_MQTT_USER = NULL: MY_MQTT_USER=mqtt_user;
  // init mqtt_password
  String(mqtt_password) == "" ? MY_MQTT_PASSWORD = NULL : MY_MQTT_PASSWORD=mqtt_password;
  return true;
}

void Task1code( void * parameter ){
  for(;;) {
    Chack_Button_Reset();
  }
}

// 创建多线程任务，用于检测GPIO 0 按钮重置
void Create_Multi_threaded_task(){
  TaskHandle_t Task1;
  xTaskCreatePinnedToCore(
        Task1code, /* Function to implement the task */
        "Task1", /* Name of the task */
        10000,  /* Stack size in words */
        NULL,  /* Task input parameter */
        0,  /* Priority of the task */
        &Task1,  /* Task handle. */
        0); /* Core where the task should run */
}

void before(){
  Serial.println("WiFi");
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  

  // 初始化 GPIO0 作为按钮输入
  Serial.println("pinMode");
  pinMode(0, INPUT);

  Serial.println("Create_Multi_threaded_task");
  Create_Multi_threaded_task();
  Serial.println("setupSpiffs");
  setupSpiffs();

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  // WiFiManager wm;

  //set config save notify callback
  Serial.println("setSaveConfigCallback");
  wm.setSaveConfigCallback(saveConfigCallback);

  // set Hostname
  wm.setHostname(MY_HOSTNAME);

  // setup custom parameters
  // 
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);

  //add all your parameters here
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_password);
  wm.addParameter(&custom_mqtt_port);

  // set static ip
  // IPAddress _ip,_gw,_sn;
  // _ip.fromString(static_ip);
  // _gw.fromString(static_gw);
  // _sn.fromString(static_sn);
  // wm.setSTAStaticIPConfig(_ip, _gw, _sn);

  //reset settings - wipe credentials for testing
  //wm.resetSettings();

  //automatically connect using saved credentials if they exist
  //If connection fails it starts an access point with the specified name
  //here  "AutoConnectAP" if empty will auto generate basedcon chipid, if password is blank it will be anonymous
  //and goes into a blocking loop awaiting configuration
  if (!wm.autoConnect(WIFI_HOT_SPOT_SSID, WIFI_HOT_SPOT_PASSWORD)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    // if we still have not connected restart and try all over again
    ESP.restart();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;
    json["mqtt_port"]   = mqtt_port;

    json["ip"]          = WiFi.localIP().toString();
    json["gateway"]     = WiFi.gatewayIP().toString();
    json["subnet"]      = WiFi.subnetMask().toString();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
  Serial.println(WiFi.dnsIP());

  // 应用 MQTT 配置
  if (!MY_Mqtt_Config()){
    // 如果 MQTT 地址解析错误，系统清除设置重启
    wm.resetSettings();
    delay(3000);
    ESP.restart();
  }
}

void setup() {
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
}