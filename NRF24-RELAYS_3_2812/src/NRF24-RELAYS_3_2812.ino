//#define MY_DEBUG

/*Raido*/
#define MY_RADIO_RF24 // Enable and select radio type attached 
#define MY_RF24_PA_LEVEL RF24_PA_MAX //MINI LOW HIGH MAX
//#define MY_RF24_CHANNEL  125


#define MY_BAUD_RATE 9600 //115200 19200 9600 =8MHZ  4800 =1MHZ

/*Node ID & NAME*/
//#define MY_NODE_ID 37  //Fallback NodeId
//#define MY_TRANSPORT_UPLINK_CHECK_DISABLED
//#define MY_PARENT_NODE_IS_STATIC //only for clone si24r1
//#define MY_PARENT_NODE_ID 0 //only for clone si24r1
char SKETCH_NAME[] = "Relay Actuator";
char SKETCH_VERSION[] = "1.5";

/*OTA Featuer*/
#define MY_OTA_FIRMWARE_FEATURE

/*SOFT PASSWORD AUTH */  //简单加密，必须要求双向，即网关节点全部采用同一标准
//#define MY_SECURITY_SIMPLE_PASSWD "FUCKYOU"

// Enabled repeater feature for this node  //注释下列启用中继功能
#define MY_REPEATER_FEATURE

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
/*
  #define CHILD_ID_IR 100 //Lock Sensor
  #define CHILD_ID_LOCK 110 //Lock Sensor
  #define CHILD_ID_HVAC 200 //HVAC Sensor
  #define CHILD_ID_VBAT 254 //BATTERY
*/

/*BATTERY&PIN*/
#define BATTERY_SENSE_PIN  A1 // battery sensor
#define LED_PIN  8  //battery  sensor
#define RELAY1_PIN 7
#define RELAY2_PIN 6
#define RELAY3_PIN 5 // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define SWITCH1_PIN A2  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define SWITCH2_PIN A3
#define SWITCH3_PIN A4  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define RELAY_ON 1  // GPIO value to write to turn on attached relay
#define RELAY_OFF 0 // GPIO value to write to turn off attached relay

/*Time Period */
bool state1;
bool state2;
bool state3;
// static int16_t value1;
// static int16_t value2;
// static int16_t value3;
static int16_t oldValue1;
static int16_t oldValue2;
static int16_t oldValue3;
int num ;
int mode;

#include <MySensors.h>
#include <TimeLib.h>
#include <Bounce2.h>
#include <FastLED.h>

/*STATUS LED DATA PIN - 2812*/
#define DATA_PIN 3 // L1
#define NUM_LEDS 3 // LED 数量
CRGB leds[NUM_LEDS]; //初始化 leds
CRGB ON_Color = CRGB::Coral; // 珊瑚色
CRGB OFF_Color= CRGB::CornflowerBlue; // 菊蓝
bool Bool_Status_Light_1;
bool Bool_Status_Light_2;
bool Bool_Status_Light_3;

// 2812 LED SETTING
#define LED_TYPE WS2812
#define COLOR_ORDER RGB
#define BRIGHTNESS  64

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
// MyMessage msgSetRgbColor( CHILD_SET_RGB_COLOR , V_RGB ); // RGB参数

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
  /*读取EEPROM 继电器数量 模式数据  */
  num = loadState(1);
  mode = loadState(2) ;
  if (num > 3 || mode > 1 ) {
    num = 1;
    mode = 0 ;
  }

  /*Led Build*/
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(SWITCH1_PIN, INPUT);
  pinMode(SWITCH2_PIN, INPUT);
  pinMode(SWITCH3_PIN, INPUT);
  digitalWrite(SWITCH1_PIN, HIGH);
  digitalWrite(SWITCH2_PIN, HIGH);
  digitalWrite(SWITCH3_PIN, HIGH);
  digitalWrite(RELAY1_PIN, loadState(CHILD_ID_RELAY1) ? RELAY_ON : RELAY_OFF);
  loadState(CHILD_ID_RELAY1) ? changStatusLed(Bool_Status_Light_1, false) : changStatusLed(Bool_Status_Light_1, true);
  digitalWrite(RELAY2_PIN, loadState(CHILD_ID_RELAY2) ? RELAY_ON : RELAY_OFF);
  loadState(CHILD_ID_RELAY2) ? changStatusLed(Bool_Status_Light_2, false) : changStatusLed(Bool_Status_Light_2, true);
  digitalWrite(RELAY3_PIN, loadState(CHILD_ID_RELAY3) ? RELAY_ON : RELAY_OFF);
  loadState(CHILD_ID_RELAY3) ? changStatusLed(Bool_Status_Light_3, false) : changStatusLed(Bool_Status_Light_3, true);

  debouncer1.attach(SWITCH1_PIN);
  debouncer1.interval(5);
  debouncer2.attach(SWITCH2_PIN);
  debouncer2.interval(5);
  debouncer3.attach(SWITCH3_PIN);
  debouncer3.interval(5);

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
  state1 = loadState(CHILD_ID_RELAY1);
  state2 = loadState(CHILD_ID_RELAY2);
  state3 = loadState(CHILD_ID_RELAY3);
  send(msgrelay1.set(loadState(CHILD_ID_RELAY1) ? RELAY_ON : RELAY_OFF));
  if ( num > 1 ) {
    send(msgrelay2.set(loadState(CHILD_ID_RELAY2) ? RELAY_ON : RELAY_OFF));
  }
  if ( num > 2 ) {
    send(msgrelay3.set(loadState(CHILD_ID_RELAY3) ? RELAY_ON : RELAY_OFF));
  }
  requestTime();
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
}

void loop()
{
  debouncer1.update();
  debouncer2.update();
  debouncer3.update();
  bool value1 = debouncer1.read();
  bool value2 = debouncer2.read();
  bool value3 = debouncer3.read();

  //SWitch模式操作
  if ( mode == 0) {
    // Get the update value
    if (value1 != oldValue1 ) {
      changeState(CHILD_ID_RELAY1, state1 ? RELAY_OFF : RELAY_ON);
    }
    oldValue1 = value1 ;

    if (value2 != oldValue2 && num > 1) {
      changeState(CHILD_ID_RELAY2, state2 ? RELAY_OFF : RELAY_ON);
    }
    oldValue2 = value2;

    if (value3 != oldValue3 && num > 2 ) {
      changeState(CHILD_ID_RELAY3, state3 ? RELAY_OFF : RELAY_ON);
    }
    oldValue3 = value3;
  }

  //button模式操作
  if ( mode == 1 ) {
    if (value1 != oldValue1 && value1 == 0) {
      changeState(CHILD_ID_RELAY1, state1 ? RELAY_OFF : RELAY_ON);
    }
    oldValue1 = value1;
    if (value2 != oldValue2 && value2 == 0 && num > 1 ) {
      changeState(CHILD_ID_RELAY2, state2 ? RELAY_OFF : RELAY_ON);
    }
    oldValue2 = value2;
    if (value3 != oldValue3 && value3 == 0 && num > 2) {
      changeState(CHILD_ID_RELAY3, state3 ? RELAY_OFF : RELAY_ON);
    }
    oldValue3 = value3;
  }

  // 状态灯更新
  Bool_Status_Light_1  ? ChangeListener(leds[SWITCH1_LED],OFF_Color) : ChangeListener(leds[SWITCH1_LED],ON_Color) ;
  Bool_Status_Light_2  ? ChangeListener(leds[SWITCH2_LED],OFF_Color) : ChangeListener(leds[SWITCH2_LED],ON_Color) ;
  Bool_Status_Light_3  ? ChangeListener(leds[SWITCH3_LED],OFF_Color) : ChangeListener(leds[SWITCH3_LED],ON_Color) ;

  timeupdate();
}


void changeState(int childId, int newState) {
  switch (childId) {
    case CHILD_ID_RELAY1:
      digitalWrite(RELAY1_PIN, newState);
      state1 = newState;
      changStatusLed(Bool_Status_Light_1, !newState); //LED反向指令
      saveState(CHILD_ID_RELAY1, state1);
      wait(20);
      send(msgrelay1.set(newState));
      break;
    case CHILD_ID_RELAY2:
      digitalWrite(RELAY2_PIN, newState);
      state2 = newState;
      changStatusLed(Bool_Status_Light_2, !newState); //LED反向指令
      saveState(CHILD_ID_RELAY2, state2);
      wait(20);
      send(msgrelay2.set(newState));
      break;
    case CHILD_ID_RELAY3:
      digitalWrite(RELAY3_PIN, newState);
      state3 = newState;
      changStatusLed(Bool_Status_Light_3, !newState); //LED反向指令
      saveState(CHILD_ID_RELAY3, state3);
      wait(20);
      send(msgrelay3.set(newState));
      break;
    default:
      break;
  }
}

void receive(const MyMessage &message) {
  // We only expect one type of message from controller. But we better check anyway.
  if (message.type == V_STATUS && message.sensor != 7  && !mGetAck(message)) {
    changeState(message.sensor, message.getBool() ? RELAY_ON : RELAY_OFF);
  }
  if (message.type == V_SCENE_ON && message.sensor != 80  && !mGetAck(message)) {
    switch (message.sensor) {
      case 81:
        changeState(message.sensor, !state1);
        break;
      case 82:
        changeState(message.sensor, !state2);
        break;
      case 83:
        changeState(message.sensor, !state3);
        break;
      default:
        break;
    }
  }

  if (message.type == V_VAR1  && message.sensor == 23) {
    char* buf = new char[strlen((message.data)) + 1];
    strcpy(buf, (message.data));
    sscanf(buf, "%d:%d", &num, &mode);
    if ( num > 3 || mode > 1 ) {
      num = 1;
      mode = 0 ;
    }
    else {
      saveState(1, num);
      saveState(2, mode);
    }
    String str = String(num) + ":" + String( mode);
    const char* value = str.c_str();
    send(msgvar1.set(value));
  }
}

void receiveTime(unsigned long ts) {
  setTime(ts);
}

void timeupdate() {
  if ((hour() == 0 && minute() == 0 && second() == 0  ) | (hour() == 12 && minute() == 0 && second() == 0  )) {
    wait(500);
    requestTime();
  }
}

/* Status LED*/
void changStatusLed(bool &Status_Light, int action){
  action == 1 ? Status_Light = true : Status_Light = false;
}

/* 切换灯光颜色，渐变效果 */
void ChangeListener(CRGB &Src_Color, CRGB &Dst_Color){
  // 如果当前当前颜色与目标颜色一致，直接 return 返回
  if (Src_Color == Dst_Color){return;}
  /// 首先降低原先 LED 灯光亮度
  /// 放入主循环中，不能延迟，每5毫秒执行一次。
  EVERY_N_MILLISECONDS(5){
    if (!Src_Color){
      Src_Color.fadeToBlackBy(10);
      FastLED.show();
    }
  }
  /// 等比例增加亮度到指定颜色
  /// 放入主循环中，不能延迟，每10毫秒执行一次
  EVERY_N_MILLISECONDS(10){
    if (Src_Color != Dst_Color){
      if (Src_Color.r != Dst_Color.r){
        Src_Color.r < Dst_Color.r ?  Src_Color.r++ : Src_Color.r -- ;
      }
      if (Src_Color.g != Dst_Color.g){
        Src_Color.g < Dst_Color.g ?  Src_Color.g++ : Src_Color.g -- ;
      }
      if (Src_Color.b != Dst_Color.b){
        Src_Color.b < Dst_Color.b ?  Src_Color.b++ : Src_Color.b -- ;
      }
      FastLED.show();
    }
  }
}