/*
 * Плавное включение и выключение подсветки кухонной зоны
 * by Welcome
 * 
 * v.0.1 - 17.10.2020
 * Реализация ШИМ на встроенном светодиоде
 * analogWrite 0-1023
 * Логика инверсная: 0 = max, 1023 = min
 */
#define LED_PIN 2

//const int linearPwmPoints[] = {0,1,2,4,8,16,32,64,128,256,512,1023};
const int linearPwmPoints[] = {0,1,3,5,8,13,21,34,55,89,144,233,377,610,987}; //Fibonachi


void setup() 
{
  pinMode(LED_PIN, OUTPUT);   
}

void loop() 
{
  for (int i = 0; i <= (sizeof(linearPwmPoints)/sizeof(int)) - 1; i++) {
    analogWrite(LED_PIN, 1023-linearPwmPoints[i]);
    delay(100);
  }
  for (int i = (sizeof(linearPwmPoints)/sizeof(int)) - 1; i >= 0; i--) {
    analogWrite(LED_PIN, 1023-linearPwmPoints[i]);
    delay(100);
  }
}
