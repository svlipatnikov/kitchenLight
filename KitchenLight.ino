/*
 * Плавное включение и выключение подсветки кухонной зоны
 * by Welcome
 * 
 * v.0.1 - 17.10.2020
 * Реализация ШИМ на встроенном светодиоде
 */
#define LED_PIN 1

void setup() {
  Serial.begin( 9600 );
  pinMode(LED_PIN, OUTPUT);
}

void loop() 
{
  digitalWrite(LED_PIN, HIGH);
  delay(2000);
  digitalWrite(LED_PIN, LOW);
  delay(2000);
}
