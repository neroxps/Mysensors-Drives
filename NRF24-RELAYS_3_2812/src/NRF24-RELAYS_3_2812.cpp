#define MY_DEBUG

/*Raido*/
#define MY_RADIO_RF24 // Enable and select radio type attached 
#define MY_RF24_PA_LEVEL RF24_PA_MAX //MINI LOW HIGH MAX
#define MY_RF24_IRQ_PIN 2

#define MY_BAUD_RATE 9600 //115200 19200 9600 =8MHZ  4800 =1MHZ

/*Node ID & NAME*/
//#define MY_NODE_ID 37  //Fallback NodeId
//#define MY_TRANSPORT_UPLINK_CHECK_DISABLED
//#define MY_PARENT_NODE_IS_STATIC //only for clone si24r1
//#define MY_PARENT_NODE_ID 0 //only for clone si24r1
char SKETCH_NAME[] = "Relay Actuator";
char SKETCH_VERSION[] = "1.9";

/*OTA Featuer*/
#define MY_OTA_FIRMWARE_FEATURE
//#define MY_SECURITY_SIMPLE_PASSWD "FUCKYOU"  //简单加密，必须要求双向，即网关节点全部采用同一标准
// 开启中继功能
#define MY_REPEATER_FEATURE
// 增加等待连接超时，允许离线启动
#define MY_TRANSPORT_WAIT_READY_MS 5000 
#define MY_RX_MESSAGE_BUFFER_FEATURE
#define MY_RX_MESSAGE_BUFFER_SIZE 10 //8MHZ 5-10 no-define 20  no-irq 3
//#define MY_RF24_BASE_RADIO_ID 0x00,0xFC,0xE1,0xA8,0xA8 //多网关模式节点
/*Child ID*/
/*
  #define CHILD_ID_DOOR 1
  #define CHILD_ID_LIGHT 2 //Light Sensor
  #define CHILD_ID_TEMP 3 //Temperature Sensor
  #define CHILD_ID_HUM 4 //Humidity Sensor
  #define CHILD_ID_BARO 5 //Pressure Sensor
  #define CHILD_ID_UV 6 //UV sensor
  #define CHILD_ID_MOTION 7 //Motion Sensor
*/
#define CHILD_ID_RELAY1 81 //Relay
#define CHILD_ID_RELAY2 82 //Relay
#define CHILD_ID_RELAY3 83 //Relay
#define CHILD_ID_LIGHT  80
/*
  #define CHILD_ID_IR 100 //Lock Sensor
  #define CHILD_ID_LOCK 110 //Lock Sensor
  #define CHILD_ID_HVAC 200 //HVAC Sensor
  #define CHILD_ID_VBAT 254 //BATTERY
*/

/*BATTERY&PIN*/

#define  LED_PIN  8  //battery  sensor
#define RELAY1_PIN A0
#define RELAY2_PIN A1
#define RELAY3_PIN A2 // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define SWITCH1_PIN A3  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define SWITCH2_PIN A4
#define SWITCH3_PIN A5  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define RELAY_ON 1  // GPIO value to write to turn on attached relay
#define RELAY_OFF 0 // GPIO value to write to turn off attached relay

/*Time Period */
bool state1, state2, state3;
bool oldValue1, oldValue2, oldValue3;
int num, mode;

char RGB_ON[7] = "ffffff";
char RGB_OFF[7] = "ffffff";
int r, g, b;
String hexstring;
static int16_t requestedLevel;
static int16_t currentLevel;

#include <MySensors.h>
#include <Bounce2.h>
#include <FastLED.h>

/*STATUS LED DATA PIN - 2812*/
#define DATA_PIN 3 // L1
#define NUM_LEDS 8 // LED 数量
CRGB leds[NUM_LEDS]; //初始化 leds
CRGB ON_Color = CRGB::Coral; // 珊瑚色
CRGB OFF_Color = CRGB::CornflowerBlue; // 菊蓝
bool Bool_Status_Light_1;
bool Bool_Status_Light_2;
bool Bool_Status_Light_3;

// 2812 LED SETTING
#define LED_TYPE WS2812B
#define COLOR_ORDER RGB
int BRIGHTNESS = 64;

// 定义状态灯编号，按照灯带顺序
#define SWITCH1_LED 0 // 1 Light
#define SWITCH2_LED 1 // 2 Light
#define SWITCH3_LED 2 // 3 Light

#define CHILD_SET_RGB_COLOR 0

Bounce debouncer1 = Bounce();
Bounce debouncer2 = Bounce();
Bounce debouncer3 = Bounce();

///*Message Instance */
MyMessage msgrelay1(CHILD_ID_RELAY1, V_STATUS);
MyMessage msgrelay2(CHILD_ID_RELAY2, V_STATUS);
MyMessage msgrelay3(CHILD_ID_RELAY3, V_STATUS);
MyMessage msgvar1(S_CUSTOM, V_VAR1); //relays数量
MyMessage lightMsg(CHILD_ID_LIGHT, V_LIGHT);
MyMessage rgbMsg(CHILD_ID_LIGHT, V_RGB);
MyMessage dimmerMsg(CHILD_ID_LIGHT, V_DIMMER);

void blinkity(uint8_t pulses, uint8_t repetitions) {
  for (int x = 0; x < repetitions; x++) {
    for (int i = 0; i < pulses; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(20);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
    delay(100);
  }
}


void before() {
  /*PWM LED Initialize*/
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);

  pinMode(SWITCH1_LED, OUTPUT);

  pinMode(LED_PIN, OUTPUT);
  debouncer1.attach(SWITCH1_PIN, INPUT_PULLUP);
  debouncer1.interval(20);
  debouncer2.attach(SWITCH2_PIN, INPUT_PULLUP);
  debouncer2.interval(20);
  debouncer3.attach(SWITCH3_PIN, INPUT_PULLUP);
  debouncer3.interval(20);

  /*读取EEPROM 继电器数量 模式数据  */
  num = loadState(1);
  mode = loadState(2) ;
  if (num > 3 || mode > 1 ) {
    num = 1;
    mode = 0 ;
  }
  /*回写继电器状态 */
  state1 = loadState(5);
  state2 = loadState(6);
  state3 = loadState(7);

  currentLevel = loadState(10);

  //  CRGB ON_Color = 0xFF017F;
  //  CRGB OFF_Color = 0xFF007F;
  digitalWrite(RELAY1_PIN, state1);
  digitalWrite(RELAY2_PIN, state2);
  digitalWrite(RELAY3_PIN, state3);
}

void setup() {
  /*检测当前2.4G链接并输出BLINK-LED */
  if (transportCheckUplink() == false) {
    blinkity(4, 2);
  }
  if (isTransportReady() == true)  {
    blinkity(2, 1);
  }

  /*将读取的EEPROM 继电器数量 模式数据发送服务器  */
  //  Serial.print("num:");
  //  Serial.println(num);
  //  Serial.print("mode:");
  //  Serial.println(mode);
  String str = String(num) + ":" + String( mode);
  const char* value = str.c_str();
  send(msgvar1.set(value));

  /*回写继电器状态并发送服务器  */

  send(msgrelay1.set(loadState(5) ? RELAY_ON : RELAY_OFF));
  if ( num > 1 ) {
    send(msgrelay2.set(loadState(6) ? RELAY_ON : RELAY_OFF));
  }
  if ( num > 2 ) {
    send(msgrelay3.set(loadState(7) ? RELAY_ON : RELAY_OFF));
  }
  digitalWrite(LED_PIN, HIGH);

  /*Led Build*/
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(currentLevel);

  /*递交WS2812RGB调光面板接口属性值*/
  send(dimmerMsg.set(currentLevel));
  wait(50);
  if (currentLevel != 0) {
    send(lightMsg.set("1"));
  }
  else {
    send(lightMsg.set("0"));
  }
  send(rgbMsg.set("000000"));
}

void presentation() {
  sendSketchInfo(SKETCH_NAME, SKETCH_VERSION);
  wait(50);
  present(CHILD_ID_RELAY1, S_BINARY, "Relay1" );
  wait(50);
  if ( num > 1) {
    present(CHILD_ID_RELAY2, S_BINARY, "Relay2" );
    wait(50);
  }
  if ( num > 2) {
    present(CHILD_ID_RELAY3, S_BINARY, "Relay3" );
  }
  wait(50);
  present(S_CUSTOM, V_VAR1, "Relay:Mode");
  wait(50);
  present(CHILD_ID_LIGHT, S_RGB_LIGHT, "RGB");
  wait(50);
  present(CHILD_ID_LIGHT, S_DIMMER, "Dimmer" );
}

void loop() {
  boolean changed1 = debouncer1.update();
  boolean changed2 = debouncer2.update();
  boolean changed3 = debouncer3.update();
  int value1 = debouncer1.read();
  int value2 = debouncer2.read();
  int value3 = debouncer3.read();
  /*SWitch模式操*/
  if ( mode == 0) {
    if (changed1) {
      changeState(CHILD_ID_RELAY1, !state1);
    }
    if (changed2) {
      changeState(CHILD_ID_RELAY2, !state2);
    }
    if (changed3) {
      changeState(CHILD_ID_RELAY3, !state3);
    }
  }
  /*button模式操作*/
  if ( mode == 1 ) {
    if (changed1 && value1 == 0) {
      changeState(CHILD_ID_RELAY1, !state1);
    }
    if (changed2 &&  value2 == 0) {
      changeState(CHILD_ID_RELAY2, !state2);
    }
    if (changed3 && value3 == 0) {
      changeState(CHILD_ID_RELAY3, !state3);
    }
  }

  // 状态灯更新

  Bool_Status_Light_1  ? ChangeListener(leds[SWITCH1_LED], OFF_Color) : ChangeListener(leds[SWITCH1_LED], ON_Color) ;
  if ( num > 1) {
    Bool_Status_Light_2  ? ChangeListener(leds[SWITCH2_LED], OFF_Color) : ChangeListener(leds[SWITCH2_LED], ON_Color) ;
  }
  if ( num > 2) {
    Bool_Status_Light_3  ? ChangeListener(leds[SWITCH3_LED], OFF_Color) : ChangeListener(leds[SWITCH3_LED], ON_Color) ;
  }
  dimmer();
}


void changeState(int childId, int newState) {
  switch (childId) {
    case CHILD_ID_RELAY1:
      wait(20);
      send(msgrelay1.set(newState));
      digitalWrite(RELAY1_PIN, newState);
      state1 = newState;
      saveState(5, state1);
      changStatusLed(Bool_Status_Light_1, !newState); //LED反向指令
      break;
    case CHILD_ID_RELAY2:
      wait(20);
      send(msgrelay2.set(newState));
      digitalWrite(RELAY2_PIN, newState);
      state2 = newState;
      saveState(6, state2);
      changStatusLed(Bool_Status_Light_2, !newState); //LED反向指令
      break;
    case CHILD_ID_RELAY3:
      wait(20);
      send(msgrelay3.set(newState));
      digitalWrite(RELAY3_PIN, newState);
      state3 = newState;
      saveState(7, state3);
      changStatusLed(Bool_Status_Light_3, !newState); //LED反向指令
      break;
    default:
      break;
  }
}

void receive(const MyMessage & message) {
  // We only expect one type of message from controller. But we better check anyway.
  if (message.type == V_STATUS && message.sensor != 7  ) {
    changeState(message.sensor, message.getBool() ? RELAY_ON : RELAY_OFF);
  }
  if (message.type == V_SCENE_ON && message.sensor != 80 ) {
    switch (message.sensor) {
      case CHILD_ID_RELAY1: changeState(message.sensor, !state1); break;
      case CHILD_ID_RELAY2: changeState(message.sensor, !state2); break;
      case CHILD_ID_RELAY3: changeState(message.sensor, !state3); break;
      default: break;
    }
  }
  if (message.type == V_VAR1  && message.sensor == 23) {
    char* buf = new char[strlen((message.data)) + 1];
    strcpy(buf, (message.data));
    sscanf(buf, "%d:%d", &num, &mode);
    if ( num > 3 || mode > 1 ) {
      int num = 1;
      int mode = 0 ;
    }
    else {
      saveState(1, num);
      saveState(2, mode);
    }
    String str = String(num) + ":" + String( mode);
    const char* value = str.c_str();
    send(msgvar1.set(value));
  }

  if (message.sensor == 80) {
    //  Retrieve the power or dim level from the incoming request message
    if (message.type == V_RGB) {
      String hexstring = message.getString();
      hexstring.toCharArray(RGB_ON, sizeof(RGB_ON));
      hexstring = String("0x") + hexstring;
      send(rgbMsg.set(RGB_ON));
      Serial.println(hexstring);
    }

    if (message.type == V_LIGHT ) {
      requestedLevel = ( message.getBool() > 0 ? 100 : 0 );
      send(lightMsg.set(requestedLevel > 0));
    }

    if (message.type == V_PERCENTAGE) {
      requestedLevel = atoi( message.data );
      send(dimmerMsg.set(requestedLevel));
      saveState(10, requestedLevel);
    }
  }

}

void dimmer() {
  if (requestedLevel != currentLevel) {
    currentLevel < requestedLevel ?  currentLevel++ : currentLevel -- ;
    FastLED.setBrightness(currentLevel);
    FastLED.show();
    wait(10);
  }
}

byte hextoint (byte c) {
  if ((c >= '0') && (c <= '9')) return c - '0';
  if ((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
  if ((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
  return 0;
}

/* Status LED*/
void changStatusLed(bool &Status_Light, int action) {
  action == 1 ? Status_Light = true : Status_Light = false;
}

/* 切换灯光颜色，渐变效果 */
void ChangeListener(CRGB &Src_Color, CRGB &Dst_Color) {
  // 如果当前当前颜色与目标颜色一致，直接 return 返回
  if (Src_Color == Dst_Color) {
    return;
  }
  /// 首先降低原先 LED 灯光亮度
  /// 放入主循环中，不能延迟，每5毫秒执行一次。
  EVERY_N_MILLISECONDS(5) {
    if (!Src_Color) {
      Src_Color.fadeToBlackBy(10);
      FastLED.show();
    }
  }
  /// 等比例增加亮度到指定颜色
  /// 放入主循环中，不能延迟，每10毫秒执行一次
  EVERY_N_MILLISECONDS(10) {
    if (Src_Color != Dst_Color) {
      if (Src_Color.r != Dst_Color.r) {
        Src_Color.r < Dst_Color.r ?  Src_Color.r++ : Src_Color.r -- ;
      }
      if (Src_Color.g != Dst_Color.g) {
        Src_Color.g < Dst_Color.g ?  Src_Color.g++ : Src_Color.g -- ;
      }
      if (Src_Color.b != Dst_Color.b) {
        Src_Color.b < Dst_Color.b ?  Src_Color.b++ : Src_Color.b -- ;
      }
      FastLED.show();
    }
  }
}