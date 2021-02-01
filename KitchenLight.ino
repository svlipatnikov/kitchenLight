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
const char* mqtt_client_name = "ESP_kitchenLight";    // Имя клиента MQTT


//переменные
byte currentBrightnes = OFF;              // текущая яркость
byte targetBrightnes = OFF;               // заданная яркость
byte manualBrightnes = OFF;               // яркость через mqtt
bool manual_mode_flag = false;            // признак управления через MQTT
bool motion_flag = false;                 // признак наличия движения
bool night_flag = false;                  // признак ночи

unsigned long lastOnlineTime;             // время когда модуль был онлайн
unsigned long lastCheckTime;              // время крайней проверки подключения к сервисам
unsigned long motionTime;                 // время срабатывания датчика движения
unsigned long lastStepTime;               // время крайнего измнения ШИМ ленты
unsigned long manualModeTime;             // время получения запроса через MQTT 
unsigned long dayTime;                    // время когда крайний раз был день

// константы
const int CHECK_PERIOD = 2 *  1000;       // периодичность проверки на подключение к сервисам
const int RESTART_PERIOD = 30*60*1000;    // время до ребута, если не удается подключиться к wi-fi
const int LIGHT_ON_TIME = 20 * 1000;      // длительность подсветки после пропадания движения
const int PWM_TIME_STEP = 10;             // время изменения значения ШИМ 
const int MANUAL_TIME = 5 * 60 * 1000;    // время в ручном режиме
const int NIGHT_TIMER = 1 *60 * 1000;     // время для фиксации признака ночь

const int  linearPwmPoints[] = {0,1,2,3,4,5,6,7,8,9,
                                10,12,14,16,18,20,23,26,29,32,
                                36,40,44,49,54,60,66,73,81,90,
                                100,110,121,134,148,163,180,198,218,240,
                                265,292,322,355,391,431,475,523,576,634,
                                698,768,845,930,1023}; // y=x*1.1
const byte iMaxBrightnes = sizeof(linearPwmPoints)/sizeof(int) - 1;            // максимальная яркость - индекс последнего элемента массива
const byte iMinBrightnes = 0;                                                  // минимальная яркость - индекс первого элемента массива

//топики 
const char topicTargetBrt[] = "/sv.lipatnikov@gmail.com/light/targetBrt";
const char topicTargetBrt_ctrl[] = "/sv.lipatnikov@gmail.com/light/targetBrt_ctrl";



//=========================================================================================
void setup() 
{
  pinMode(PIN_led_strip, OUTPUT); 
  pinMode(PIN_motion_sensor1, INPUT);  
  pinMode(PIN_motion_sensor2, INPUT);  
  pinMode(PIN_light_sensor, INPUT); 

  Connect_WiFi(IP_KitchenLight, NEED_STATIC_IP);
  Connect_OTA();            
             
  Connect_mqtt(mqtt_client_name);
  MQTT_subscribe();

  targetBrightnes = OFF;
  ledStripControl(); //выключаем ленту
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

  // сбрасываем флаг ручного режима через MANUAL_TIME
  if ((long)millis() - manualModeTime > MANUAL_TIME)
    manual_mode_flag = false; 

  // управление светодиодной лентой
  if (manual_mode_flag) 
    targetBrightnes = manualBrightnes;
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

//функция управления светодиодной лентой
void ledStripControl () 
{
  if ((long)millis() - lastStepTime > PWM_TIME_STEP) {
    lastStepTime = millis(); 

    if (targetBrightnes > currentBrightnes) currentBrightnes++;      
    if (targetBrightnes < currentBrightnes) currentBrightnes--;
    currentBrightnes = constrain(currentBrightnes, 0 , 100);
      
    analogWrite(PIN_led_strip, linearPwmPoints[getLightIndex(currentBrightnes)]);
  }
}

// функция преобразования яркости в процентах в индекс массива linearPwmPoints
byte getLightIndex (byte percent) {
  byte index = (byte)(iMaxBrightnes * percent / 100); 
  return constrain(index, iMinBrightnes, iMaxBrightnes); // проверка на диапазон
}


//=========================================================================================
//функции MQTT

// функция подписки на топики 
void MQTT_subscribe(void) {
  client.subscribe(topicTargetBrt_ctrl);      
}

// получение данных от сервера
void mqtt_get(char* topic, byte* payload, unsigned int length) 
{
  // создаем копию полученных данных
  char localPayload[length + 1];
  for (int i=0; i<length; i++) { localPayload[i] = (char)payload[i]; }
  localPayload[length] = 0;

  // присваиваем переменным значения в зависимости от топика 
  if (strcmp(topic, topicTargetBrt_ctrl) == 0) { 
    int ivalue = 0; sscanf(localPayload, "%d", &ivalue);
    manualBrightnes = (byte)ivalue;
    manual_mode_flag = true;
    manualModeTime = millis(); 
    MQTT_publish_int(topicTargetBrt, targetBrightnes); 
  }
}
