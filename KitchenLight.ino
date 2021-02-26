/*
 * Плавное включение и выключение подсветки кухонной зоны
 * by Sergey Lipatnikov
 * 
 * Реализация ШИМ на встроенном светодиоде
 * ШИМ реализуется встроенной функцией analogWrite 
 * Диапазон ШИМ: 0-1023
 * 
 */

#define PIN_motion_sensor1  0  // пин подключения датчика движения №1
#define PIN_motion_sensor2  1  // пин подключения датчика движения №2
#define PIN_light_sensor    2  // пин подключения датчика света
#define PIN_led_strip       3  // пин ШИМ-сигнала для светодиодной ленты


#define  OFF          0
#define  NIGHT_LIGHT  25
#define  DAY_LIGHT    100


#include <ESP8266WiFi.h>
const char ssid[] = "welcome's wi-fi";
const char pass[] = "27101988";
const bool NEED_STATIC_IP = true;
IPAddress IP_KitchenLight      (192, 168, 1, 87);
IPAddress IP_Cristmas          (192, 168, 1, 83);  
IPAddress IP_Node_MCU          (192, 168, 1, 71);
IPAddress IP_Fan_controller    (192, 168, 1, 41);
IPAddress IP_Water_sensor_bath (192, 168, 1, 135); 
IPAddress IP_Toilet_controller (192, 168, 1, 54);


#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


#include <PubSubClient.h>
WiFiClient ESP_kitchenLight;
PubSubClient client(ESP_kitchenLight);
const char* mqtt_client_name = "light_esp8266";    // Имя клиента MQTT


//переменные
float currentBrightnes = OFF;             // текущая яркость
float targetBrightnes = OFF;              // заданная яркость
float lastTargetBrightnes = OFF;          // предыдущая яркость
float ledStartBrightnes = OFF;            // начальная яркость перед изменением на новую целевую
byte manualBrightnes = OFF;               // яркость через mqtt
bool motion_flag = false;                 // признак наличия движения
bool night_flag = false;                  // признак ночи

unsigned long lastOnlineTime;             // время когда модуль был онлайн
unsigned long lastCheckTime;              // время крайней проверки подключения к сервисам
unsigned long motionTime;                 // время срабатывания датчика движения
unsigned long lastStepTime;               // время крайнего измнения ШИМ ленты
unsigned long manualModeTime;             // время получения запроса через MQTT 
unsigned long dayTime;                    // время когда крайний раз был день
unsigned long ledStartTime;               // время начала режима изменения яркости

// константы
const int CHECK_PERIOD = 2 *  1000;       // периодичность проверки на подключение к сервисам
const int RESTART_PERIOD = 30*60*1000;    // время до ребута, если не удается подключиться к wi-fi
const int LIGHT_ON_TIME = 20 * 1000;      // длительность подсветки после пропадания движения
const int PWM_TIME_STEP = 6;              // время изменения значения ШИМ 
const int MANUAL_TIME = 30 * 60 * 1000;   // время в ручном режиме
const int NIGHT_TIMER = 1 *60 * 1000;     // время для фиксации признака ночь
const int CHANGE_TIME = 1000;             // время изменения яркости

// линейный массив ШИМ на каждые 10%
// const int  linearPwmPoints[] = {0,5,11,22,40,72,125,210,360,610,1023};  // old
// const int  linearPwmPoints[] = {0,7,20,40,75,135,220,340,500,730,1023}; // new
// const int  linearPwmPoints[] = {0,7,17,34,60,100,170,270,430,680,1023}; // new2
const int  linearPwmPoints[] = {0,10,25,50,90,155,250,380,550,770,1023}; // new3




//топики 
const char topicManualBrt[] = "user_1502445e/light/manualBrt";
const char topicManualBrt_ctrl[] = "user_1502445e/light/manualBrt_ctrl";

const char topicTest[] = "user_1502445e/light/test";
const char topicTest2[] = "user_1502445e/light/test2";

//=========================================================================================
void setup() 
{
  pinMode(PIN_led_strip, OUTPUT);    
  pinMode(PIN_motion_sensor1, INPUT);  
  pinMode(PIN_motion_sensor2, INPUT);  
  pinMode(PIN_light_sensor, INPUT); 

  // выключаем ленту
  analogWrite(PIN_led_strip, 0);

  Connect_WiFi(IP_KitchenLight, NEED_STATIC_IP);
  Connect_OTA();            
             
  Connect_mqtt(mqtt_client_name);
  MQTT_subscribe();
  MQTT_publish_int(topicManualBrt, OFF); 
  MQTT_publish_int(topicManualBrt_ctrl, OFF);

  
}

//=========================================================================================
void loop() 
{
  // сетевые функции
  httpServer.handleClient();          // для обновления по воздуху   
  client.loop();                      // для функций MQTT 

/*
  // проверка сигнала от датчиков движения
  motion_flag = getMotionFlag();

  // проверка признака ночи
  night_flag = getNightFlag();
*/

  // сбрасываем ручной режим через MANUAL_TIME
  if (((long)millis() - manualModeTime > MANUAL_TIME) && manualBrightnes) {
    manualBrightnes = 0;
    MQTT_publish_int(topicManualBrt, manualBrightnes);  
  }

  // управление светодиодной лентой
  if (manualBrightnes) 
    targetBrightnes = (float)manualBrightnes;
  else if (motion_flag && !night_flag) 
    targetBrightnes = DAY_LIGHT;
  else if (motion_flag && night_flag) 
    targetBrightnes = NIGHT_LIGHT;
  else                  
    targetBrightnes = OFF;
   
  ledStripControl();

 
  // проверка подключений к wifi и серверам
  if ((long)millis() - lastCheckTime > CHECK_PERIOD) {
    lastCheckTime = millis();     
     
    if (WiFi.status() != WL_CONNECTED) {   
      Connect_WiFi(IP_KitchenLight, NEED_STATIC_IP); // WI-FI 
      Connect_OTA();                                 // OTA
      Restart(lastOnlineTime, RESTART_PERIOD);     
    }
    else 
      lastOnlineTime = millis();    

    if (!client.connected()) {            // MQTT
      Connect_mqtt(mqtt_client_name);
      MQTT_subscribe();
    }     
  }
}

//=========================================================================================

// функция определения признака ночи
bool getNightFlag () {
  if (digitalRead(PIN_light_sensor)) 
    dayTime = millis(); // время когда крайний раз был день
  
  if ((long)millis() - dayTime < NIGHT_TIMER) 
    return true;
  else
    return false;
}

//=========================================================================================

// функция определения движения
bool getMotionFlag () {
  if (digitalRead(PIN_motion_sensor1) || digitalRead(PIN_motion_sensor2)) { 
    motionTime = millis(); 
    return true; 
  }
  else if ((long)millis() - motionTime < LIGHT_ON_TIME) 
    return true;
  else        
    return false; 
}


//=========================================================================================


bool flag = false;

// управление лентой
void ledStripControl () {  
  unsigned long currentTime = millis(); // текущее время
  
  if (targetBrightnes != lastTargetBrightnes) {
    lastTargetBrightnes = targetBrightnes;
    ledStartTime = currentTime;
    ledStartBrightnes = currentBrightnes; 

    MQTT_publish_str(topicTest, "start");
    flag = true;
  }



  if (currentTime - ledStartTime < CHANGE_TIME) {
//    float koef = currentTime - ledStartTime * 1000 / CHANGE_TIME; 
    float koef = ((currentTime - ledStartTime) * 1000 / CHANGE_TIME);
    currentBrightnes = ledStartBrightnes + (targetBrightnes - ledStartBrightnes) * koef / 1000;
    currentBrightnes = constrain(currentBrightnes, 0, 100);   
    analogWrite(PIN_led_strip, getPWMvalue(currentBrightnes));   
  }  
  else if (flag == true)
  {
    MQTT_publish_str(topicTest, "End"); 
    flag = false;
  }
}

// функция получения значения PWM из массива linearPwmPoints
int getPWMvalue(float brt) {
  byte indexMin = (byte)floor(brt/10);   
  byte indexMax = (byte)ceil(brt/10);

  float percent = (brt - (indexMin * 10)) / 10;
  int pwm = round(linearPwmPoints[indexMin] + (linearPwmPoints[indexMax] - linearPwmPoints[indexMin]) * percent);
  pwm = constrain(pwm, 0, 1023);
  return pwm;
}
