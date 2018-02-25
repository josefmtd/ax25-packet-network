/* This software is modified by Josef Matondang
 * from mkiss.ino that is made by folkert@vanheusden.com
 */

#include "kiss.h"

#include <SPI.h>
#include <RH_RF95.h>

#define pinReset 7

/* This program is a KISS-TNC implementation of LoRa
 * based on the driver RadioHead, specifically for
 * RF95 radio. Latest RadioHead library can be accessed
 * in http://www.airspayce.com/mikem/arduino/RadioHead/RadioHead-1.83.zip
 */

static RH_RF95 rf95;

/* We will be making a KISS class, an OOP approach to do the
 * KISS algorithm. All low-level callback functions will be defined
 * in this sketch, so that the KISS class can be generic
 */

bool peekRadio() {
  return rf95.available();
}

void getRadio(uint8_t *const whereTo, uint16_t *const n) {
  uint8_t dummy = *n;
  rf95.recv(whereTo, &dummy);
  *n = dummy;
}

void putRadio(const uint8_t *const what, const uint16_t size) {
  rf95.send(what, size);
  rf95.waitPacketSent();
}

uint16_t peekSerial() {
  return Serial.available();
}

bool getSerial(uint8_t *const whereTo, const uint16_t n, const unsigned long int to) {
  for (uint16_t i = 0; i < n; i++) {
    while(!Serial.available()) {
      if (millis() >= to) {
        return false;
      }
    }
    whereTo[i] = Serial.read();
  }

  return true;
}

void putSerial(const uint8_t *const what, const uint16_t size) {
  Serial.write(what, size);
}

bool initRadio() {
  if (rf95.init())  {
    delay(100);
    return true;
  }
  return false;
}

bool resetRadio() {
  digitalWrite(pinReset, LOW);
  delay(1);
  digitalWrite(pinReset, HIGH);
  delay(5 + 1);

  return initRadio();
}

kiss k(254, peekRadio, getRadio, putRadio, peekSerial, getSerial, putSerial,
  resetRadio);

void setup() {
  Serial.begin(19200);

  while(!Serial);

  pinMode(pinReset, OUTPUT);
  digitalWrite(pinReset, HIGH);

  k.begin();

  if (!resetRadio())
    k.debug("Radio init failed");
  else
    k.debug("Radio init success");
}

void loop() {
  k.loop();

  const unsigned long int now = millis();

  static unsigned long int lastReset = 0;
  const unsigned long int resetInterval = 301000;
  if (now - lastReset >= resetInterval) {
    k.debug("Reset radio");

    lastReset = now;
  }
}
