#include <SPI.h>
#include <RF24.h>
#include "printf.h"

// CE, CSN
RF24 radio(9, 10);

// 5-byte address
const byte address[5] = {'N','H','U','T','1'};

char txPayload[32] = "HELLO_NO_ACK_FROM_TX";

uint8_t nrfReadReg(uint8_t reg)
{
  digitalWrite(10, LOW);
  SPI.transfer(0x00 | (reg & 0x1F)); // R_REGISTER
  uint8_t val = SPI.transfer(0xFF);
  digitalWrite(10, HIGH);
  return val;
}

void nrfReadRegN(uint8_t reg, uint8_t* buf, uint8_t len)
{
  digitalWrite(10, LOW);
  SPI.transfer(0x00 | (reg & 0x1F));
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = SPI.transfer(0xFF);
  }
  digitalWrite(10, HIGH);
}

void dumpReg1(uint8_t reg)
{
  uint8_t val = nrfReadReg(reg);

  Serial.print("REG 0x");
  if (reg < 0x10) Serial.print("0");
  Serial.print(reg, HEX);
  Serial.print(" : ");
  if (val < 0x10) Serial.print("0");
  Serial.println(val, HEX);
}

void dumpRegN(uint8_t reg, uint8_t len)
{
  uint8_t buf[5] = {0};
  nrfReadRegN(reg, buf, len);

  Serial.print("REG 0x");
  if (reg < 0x10) Serial.print("0");
  Serial.print(reg, HEX);
  Serial.print(" : ");

  for (uint8_t i = 0; i < len; i++) {
    if (buf[i] < 0x10) Serial.print("0");
    Serial.print(buf[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}


void dumpAllRegister(void) {
  Serial.println("===== NRF24 HEX REGISTER DUMP =====");

  dumpReg1(0x00);
  dumpReg1(0x01);
  dumpReg1(0x02);
  dumpReg1(0x03);
  dumpReg1(0x04);
  dumpReg1(0x05);
  dumpReg1(0x06);
  dumpReg1(0x07);
  dumpReg1(0x08);
  dumpReg1(0x09);

  dumpRegN(0x0A, 5); // RX_ADDR_P0
  dumpRegN(0x0B, 5); // RX_ADDR_P1
  dumpRegN(0x10, 5); // TX_ADDR

  dumpReg1(0x11);
  dumpReg1(0x12);
  dumpReg1(0x13);
  dumpReg1(0x14);
  dumpReg1(0x15);
  dumpReg1(0x16);

  dumpReg1(0x17);
  dumpReg1(0x1C);
  dumpReg1(0x1D);

  Serial.println("===== END DUMP =====");
}

void setup() {
  Serial.begin(115200);
  radio.begin();
  printf_begin();

  // ===== NO ACK CONFIG =====
  Serial.println("===== NO ACK CONFIG =====");
  radio.setAutoAck(false);
  radio.setRetries(5, 4);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(32);

  radio.setChannel(0x0C);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);

  radio.openWritingPipe(address);
  radio.stopListening();            // TX mode

  dumpAllRegister();
  Serial.println("TX NO-ACK READY");
}

void loop() {
  bool ok = radio.write(&txPayload, 32);

  Serial.print("Send status: ");
  Serial.println(ok ? "OK" : "FAIL");

  delay(1000);
}
