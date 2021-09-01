#include "DHT11.h"

// Return values:
// DHTLIB_OK
// DHTLIB_ERROR_CHECKSUM
// DHTLIB_ERROR_TIMEOUT
int DHT11::Read(int pin) {
  // BUFFER TO RECEIVE
  uint8_t bits[5];
  uint8_t cnt = 7;
  uint8_t idx = 0;

  // EMPTY BUFFER
  for (int i = 0; i < 5; i++) bits[i] = 0;

  // REQUEST SAMPLE
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delay(20);//>=18ms
  digitalWrite(pin, HIGH);
  pinMode(pin, INPUT);
  delayMicroseconds(40);

  // ACKNOWLEDGE or TIMEOUT
  unsigned int loopCnt = 10000;
  while (digitalRead(pin) == LOW)
    if (loopCnt-- == 0) return DHTLIB_ERROR_TIMEOUT;

  loopCnt = 10000;
  while (digitalRead(pin) == HIGH)
    if (loopCnt-- == 0) return DHTLIB_ERROR_TIMEOUT;

  // READ OUTPUT - 40 BITS => 5 BYTES or TIMEOUT
  for (int i = 0; i < 40; i++) {
    loopCnt = 10000;
    while (digitalRead(pin) == LOW)
      if (loopCnt-- == 0) return DHTLIB_ERROR_TIMEOUT;

    unsigned long t = micros();

    loopCnt = 10000;
    while (digitalRead(pin) == HIGH)
      if (loopCnt-- == 0) return DHTLIB_ERROR_TIMEOUT;

    if ((micros() - t) > 40) bits[idx] |= (1 << cnt);
    if (cnt == 0) // next byte?
    {
      cnt = 7;    // restart at MSB
      idx++;      // next byte!
    } else cnt--;
  }

  // WRITE TO RIGHT VARS

  humidity = bits[0] * 10 + bits[1];

  if (bits[3] & 0X80)   //negative temperature
  {
    temperature = 0 - (bits[2] * 10 + ((bits[3] & 0x7F)));
  } else    //positive temperature
  {
    temperature = bits[2] * 10 + bits[3];
  }
  //temperature range：-20℃~60℃，humidity range:5％RH~95％RH
  if (humidity > 950) {
    humidity = 950;
  }
  if (humidity < 50) {
    humidity = 50;
  }
  if (temperature > 600) {
    temperature = 600;
  }
  if (temperature < -200) {
    temperature = -200;
  }
  temperature = temperature / 10;   //convert to the real temperature value
  humidity = humidity / 10;         //convert to the real humidity value

  uint8_t sum = bits[0] + bits[1] + bits[2] + bits[3];

  if (bits[4] != sum) return DHTLIB_ERROR_CHECKSUM;
  return DHTLIB_OK;
}
