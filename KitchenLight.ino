/*
 * Плавное включение и выключение подсветки кухонной зоны
 * by Sergey Lipatnikov
 * 
 * ШИМ реализуется встроенной функцией analogWrite 
 * Диапазон ШИМ: 0-1023
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


#include <NTPClient.h>
WiFiUDP Udp2;
NTPClient timeClient(Udp2, "europe.pool.ntp.org");


//переменные
float currentBrightnes = OFF;             // текущая яркость
float targetBrightnes = OFF;              // заданная яркость
float lastTargetBrightnes = OFF;          // предыдущая яркость
float ledStartBrightnes = OFF;            // начальная яркость перед изменением на новую целевую
float manualBrightnes = OFF;              // яркость через mqtt

bool motion_flag = false;                 // признак наличия движения
bool night_flag = false;                  // признак ночи
bool manual_flag = false;                 // признак руного режима
bool off_flag = false;                    // признак отключенного режима
bool ntp_night_flag = false;              // призак ночи по через ntp

unsigned long lastOnlineTime;             // время когда модуль был онлайн
unsigned long lastCheckTime;              // время крайней проверки подключения к сервисам
unsigned long motionTime;                 // время срабатывания датчика движения
unsigned long manualModeTime;             // время получения запроса через MQTT 
unsigned long dayTime;                    // время когда крайний раз был день
unsigned long ledStartTime;               // время начала режима изменения яркости
unsigned long currentTime;                // текущее время
unsigned long getFlagTime;                // время вычисления флагов
unsigned long Last_get_ntp_time;          // время крайнего получения вермени NTP

// константы
const int CHECK_PERIOD = 1 *  1000;       // периодичность проверки на подключение к сервисам
const int GET_FLAG_PERIOD = 50;           // период вычисления флагов
const int RESTART_PERIOD = 30*60*1000;    // время до ребута, если не удается подключиться к wi-fi
const int MOTION_TIMER = 60 * 1000;       // длительность подсветки после пропадания движения
const int PWM_TIME_STEP = 6;              // время изменения значения ШИМ 
const int MANUAL_TIMER = 10 * 60 * 1000;  // время в ручном режиме
const int NIGHT_TIMER = 1 *60 * 1000;     // время для фиксации признака ночь
const int UP_CHANGE_TIME = 1000;          // время изменения яркости на включение
const int DOWN_CHANGE_TIME = 5000;        // время изменения яркости на выключение
const int GET_NTP_TIME_PERIOD = 60*1000;  // период получения времни с сервера NTP

// линейный массив ШИМ на каждые 10%
const int  linearPwmPoints[] = {0,10,25,50,90,155,250,380,550,770,1023}; // new3


//топики 
const char topicManualBrt[] = "user_1502445e/light/manualBrt";
const char topicManualBrt_ctrl[] = "user_1502445e/light/manualBrt_ctrl";

const char topicOffMode[] = "user_1502445e/light/offMode";
const char topicOffMode_ctrl[] = "user_1502445e/light/offMode_ctrl";

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

  timeClient.begin();
  timeClient.setTimeOffset(4*60*60);   //смещение на UTC+4
}

//=========================================================================================
void loop() 
{
  // получение признака Ночь через NTP
  if ((long)millis() - Last_get_ntp_time > GET_NTP_TIME_PERIOD) {
    Last_get_ntp_time = millis(); 
    ntp_night_flag = getNtpNightFlag();
  } 
  
  // сетевые функции
  httpServer.handleClient();          // для обновления по воздуху   
  client.loop();                      // для функций MQTT 

  // текущее время
  currentTime = millis();             

  // получаем флаги (в каждом цикле нельзя - начинает глючить)
  if (currentTime - getFlagTime > GET_FLAG_PERIOD) {
    getFlagTime = currentTime;
    motion_flag = getMotionFlag();   // проверка сигнала от датчиков движения
    night_flag = getNightFlag();     // проверка признака ночи
  }

  // сбрасываем ручной режим через MANUAL_TIME
  if (((long)millis() - manualModeTime > MANUAL_TIMER) && manual_flag) {
    manual_flag = false;
    manualBrightnes = 0;
    MQTT_publish_int(topicManualBrt, manualBrightnes);  
  }

  // управление светодиодной лентой
  if (off_flag)
    ledStripControl(OFF);
  else if (manual_flag)
    ledStripControl(manualBrightnes);
  else if (motion_flag) {
    int brightnes = (night_flag || ntp_night_flag) ? NIGHT_LIGHT : DAY_LIGHT;
    ledStripControl(brightnes);
  }
  else                   
    ledStripControl(OFF);
   
  // проверка подключений к wifi и серверам
  if (currentTime - lastCheckTime > CHECK_PERIOD) {
    lastCheckTime = currentTime;    
    
    if (WiFi.status() != WL_CONNECTED) {   
      Connect_WiFi(IP_KitchenLight, NEED_STATIC_IP); // WI-FI 
      Connect_OTA();                                 // OTA
      Restart(lastOnlineTime, RESTART_PERIOD);     
    }
    else 
      lastOnlineTime = currentTime;    

    if (!client.connected()) {            // MQTT
      Connect_mqtt(mqtt_client_name);
      MQTT_subscribe();
    }     
  }
}

//=========================================================================================

// функции определения признака ночи
bool getNightFlag () {
  if (digitalRead(PIN_light_sensor)) dayTime = millis(); // время когда крайний раз был день 
  if (currentTime - dayTime < NIGHT_TIMER) return true;
  return false;
}

bool getNtpNightFlag () {
  timeClient.update();
  if ((timeClient.getHours() >= 22) || (timeClient.getHours() <= 6)) return true;
  return false;
}

//=========================================================================================

byte pir1_counter = 0;
byte pir2_counter = 0;

// функция определения движения
bool getMotionFlag () { 
  if (digitalRead(PIN_motion_sensor1)) 
    pir1_counter++;
  else
    pir1_counter = 0;
    
  if (digitalRead(PIN_motion_sensor2)) 
    pir2_counter++;
  else
    pir2_counter = 0;
  
  //bool pir1 = digitalRead(PIN_motion_sensor1);
  //bool pir2 = digitalRead(PIN_motion_sensor2);

  if (pir1_counter > 2 || pir2_counter > 2) 
    motionTime = currentTime;
  
  if (currentTime - motionTime < MOTION_TIMER) {
    if (!motion_flag) { 
      if (pir1_counter > 2) MQTT_publish_str(topicTest, "motion_flag pir1");
      if (pir2_counter > 2) MQTT_publish_str(topicTest, "motion_flag pir2");
    }
    return true;
  }
  else
    return false;
}


//=========================================================================================

// управление лентой
void ledStripControl (float brt) {   
  
  if (brt != targetBrightnes) {
    targetBrightnes = brt;
    ledStartTime = currentTime;
    ledStartBrightnes = currentBrightnes; 
  }

  int changeTime = (targetBrightnes > ledStartBrightnes) ? UP_CHANGE_TIME : DOWN_CHANGE_TIME;

  if (currentTime - ledStartTime < changeTime) {
    float koef = ((currentTime - ledStartTime) * 1000 / changeTime);
    currentBrightnes = ledStartBrightnes + (targetBrightnes - ledStartBrightnes) * koef / 1000;
    currentBrightnes = constrain(currentBrightnes, 0, 100);   
    analogWrite(PIN_led_strip, getPWMvalue(currentBrightnes));   
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
