/* v.02 - 01/01/2020
 *  Устраенна ошибка с первым подключением к wi-fi
 *  Уделен алгоритм увеличения времени до ребута (после рестарта он все равно обнуляется)
 *  
 *  v03 - 15/01/2020
 *  удалено управление светодиодом из функции Connect (не везде есть свободные пины)
 *  
 *  v04 - 04/02/2020
 *  Введено ограничение по времени на поключение к Blynk
 *  
 *  v05 - 18/02/2020 
 *  - static IP (не работает с Blynk, закомментирован)
 *  - упрощен алгоритм подключения к Blynk
 *  
 *  v06 - 21/02/2020
 *  - доработан Connect_WiFi функции запуска серверов перенесены внутрь временного условия
 *  
 *  v10 - 17/03/2020
 *  добавлено подключение к MQTT серверу  mqtt.by
 *  из функций удалены проверки на время крайнейнего вызова
 *  добавлены аргументы static_ip функции Connect_WiFi
 *  
 * v101 - 18/03/2020 
 * переход с mqtt.by на mqtt.dioty.co 
 * 
 * v102 - 21/03/2020
 * Удалена функция подключения к MQTT - должна быть вынесена в отдельный файл
 * 
 * v11 - 19/10/2020
 * Подключение к ОТА и UDP вынесено в отдельные функции
 * Добавлены функции MQTT
 */
 

IPAddress gateway (192, 168, 1, 1);
IPAddress mask (255, 255, 255, 255);

  
// Подключение к wi-fi 
void Connect_WiFi(IPAddress device_ip, bool static_ip){
  WiFi.mode(WIFI_STA);  
  if (static_ip) WiFi.config(device_ip, gateway, mask);   
  WiFi.begin(ssid, pass);    
  byte i=0;  while ((WiFi.status() != WL_CONNECTED) && (i<5)) { delay(1000); i++; }     // подключаемся к wi-fi или выходим через 5 сек     
} 


// web-сервер для обновления по воздуху 
void Connect_OTA (void) {  
  if (WiFi.status() == WL_CONNECTED)  {
    httpUpdater.setup(&httpServer); 
    httpServer.begin();    
  }
}


// UDP
void Connect_UDP (void) {
  //if (WiFi.status() == WL_CONNECTED)
    //Udp.begin(localPort); 
}


// Функция рестарта модуля при долгом отсутствии подключения к wi-fi
void Restart (unsigned long Online_time, const int max_offline_time) {
  if ((long)millis() - Online_time > max_offline_time) {  
    WiFi.disconnect();
    ESP.eraseConfig();
    delay(1000);
    ESP.reset();
  }
}

//===================================================================================================
// подключение к MQTT 

const char *mqtt_server = "mqtt.dioty.co";          // Имя сервера MQTT
const int   mqtt_port = 1883;                       // Порт для подключения к серверу MQTT
const char *mqtt_user = "sv.lipatnikov@gmail.com";  // Логин от сервера
const char *mqtt_pass = "83eb5858";                 // Пароль от сервера

void Connect_mqtt(const char* client_name) {
  if (WiFi.status() == WL_CONNECTED) {
    client.setServer(mqtt_server, mqtt_port);            
    if (client.connect(client_name, mqtt_user, mqtt_pass)) 
      client.setCallback(mqtt_get);
  }
}


// Функция отправки int или bool на сревер mqtt
void MQTT_publish_int(const char* topic , int data){
  char msg[5];
  sprintf (msg, "%u", data);    
  client.publish(topic, msg, true);
}


// Функция отправки float на сревер mqtt
void MQTT_publish_float(const char* topic , float data){
  char msg[4];
  sprintf (msg, "%2.1f", data);    
  client.publish(topic, msg, true);
}
