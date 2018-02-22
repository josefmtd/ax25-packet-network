#include <Arduino.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "kiss.h"

kiss::kiss(const uint16_t maxPacketSizeIn, bool (* peekRadioIn)(),
void (* getRadioIn)(uint8_t *const whereTo, uint16_t *const n),
void (* putRadioIn)(const uint8_t *const what, const uint16_t size),
uint16_t (*peekSerialIn)(), bool(* getSerialIn)(uint8_t *const whereTo,
const uint16_t n, const unsigned long int to), void (* putSerialIn)
(const uint8_t *const what, const uint16_t size), bool (* resetRadioIn)())
: bufferBig(new uint8_t[maxPacketSizeIn * 2]), bufferSmall(new uint8_t[maxPacketSizeIn]),
  maxPacketSize(maxPacketSizeIn), peekRadio(peekRadioIn), getRadio(getRadioIn),
  putRadio(putRadioIn), peekSerial(peekSerialIn), getSerial(getSerialIn),
  putSerial(putSerialIn), resetRadio(resetRadioIn) {
    debug("Starting KISS Mode");
  }

kiss::~kiss() {
  delete [] bufferSmall;
  delete [] bufferBig;
}

void kiss::begin() {
}

#define FEND 0xc0
#define FESC 0xdb
#define TFEND 0xdc
#define TFESC 0xdd

void put_byte(uint8_t *const out, uint16_t *const offset, const uint8_t c) {
  if (c == FEND) {
    out[(*offset)++] = FESC;
    out[(*offset)++] = TFEND;
  }
  else if (c == FESC) {
    out[(*offset)++] = FESC;
    out[(*offset)++] = TFESC;
  }
  else {
    out[(*offset)++] = c;
  }
}

void kiss::debug(const char *const t) {
  uint8_t myBuffer[128];
  const uint8_t ax25_ident[] = { 0x92, 0x88, 0x8A, 0x9C, 0xA8, 0x40, 0xE0, 0x88,
  0x8A, 0x84, 0xAA, 0x8E, 0x60, 0x63, 0x03, 0xF0};

  uint16_t o = 0;
  myBuffer[o++] = 0x00;

  for(uint8_t i = 0; i < sizeof(ax25_ident); i++) {
    myBuffer[o++] = ax25_ident[i];
  }

  uint8_t l = strlen(t);
  for(uint8_t i = 0; i < l; i++) {
    myBuffer[o++] = t[i];
  }

  const uint8_t fend = FEND;
  putSerial(&fend, 1);

  for(uint8_t i = 0; i < o; i++) {
    uint8_t tinyBuffer[4];
    uint16_t oo = 0;
    put_byte(tinyBuffer, &oo, myBuffer[i]);
    putSerial(tinyBuffer, oo);
  }

  putSerial(&fend, 1);
}

void kiss::processRadio() {
  uint16_t nBytes = maxPacketSize;
  getRadio(bufferSmall, &nBytes);

  //char buf[20];
  //snprintf(buf, sizeof buf, "recvradio %d", nBytes);
  //debug(buf);

  uint16_t o = 0;

  bufferBig[o++] = FEND;
  bufferBig[o++] = 0x00;
  
  for(uint16_t i = 0; i < nBytes; i++) {
    put_byte(bufferBig, &o, bufferSmall[i]);
  }

  bufferBig[o++] = FEND;
  
  putSerial(bufferBig,o);
}

void kiss::processSerial() {
  bool first = true, ok = false, escape = false;

  uint16_t o = 0;
  
  const unsigned long int end = millis() + 5000;
  for(;millis() < end;) {
    uint8_t buffer = 0;

    if (!getSerial(&buffer, 1, end)) {
      debug("ser recv to");
      break;
    }

    if (escape) {
      if (o == maxPacketSize) {
        debug("error packet size");
        break;
      }

      if (buffer == TFEND) {
        bufferSmall[o++] = FEND;
      }
      else if (buffer == TFESC) {
        bufferSmall[o++] = FESC;
      }
      else {
        debug("error escape");
      }

      escape = false;
    }
    else if (buffer == FEND)
    {
      if (first) {
        first = false;

        if (o) {
          debug("ign garbage");
          o = 0;
        }
      }
      else {
        ok = true;
        break;
      }
    }
    else if (buffer == FESC) {
      escape = true;
    }
    else {
      if (o == maxPacketSize) {
        debug("error packet size2");
        break;
      }

      bufferSmall[o++] = buffer;
    }
  }

  if (ok) {
    uint8_t cmd = bufferSmall[0] & 0x0F;

    if (cmd == 0x00 && o >= 2) {
      if (o > 0)
        putRadio(&bufferSmall[1], o);
    }

    else if ((bufferSmall[0] & 0x0F) == 0x0F) {
      if(!resetRadio()) {
        debug("Reset radio failed");
        ok = false;
      }
    }

    else {
      char err[64];
      snprintf(err, sizeof(err), "frame type %02x unk", bufferSmall[0]);
      debug(err);
      ok = false;
    }
  }

  // static uint16_t cnt = 0;
  // char buf[16];
  //
  // if (ok)
  //   snprintf(buf, sizeof buf, "OK %d", ++cnt);
  // else
  //   snprintf(buf, sizeof buf, "FAIL %d", ++cnt);
  //
  // buf[sizeof(buf) - 1] = 0x00;
  //
  // debug(buf);
}

void kiss::loop() {
  if (peekRadio()) {
    processRadio();
  }

  bool serialIn = peekSerial();
  if (serialIn) {
    processSerial();
  }
}
