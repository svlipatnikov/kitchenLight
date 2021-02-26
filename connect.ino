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
