#include <SPI.h>
#include <RF24.h>

// CE, CSN
RF24 radio(9, 10);

const byte address[5] = {'N','H','U','T','1'};
char rxBuffer[32];

void dump_nrf24_registers(RF24 &radio)
{
  uint8_t buf[5];

  Serial.println(F("========== nRF24L01 REGISTER DUMP =========="));

  Serial.print(F("CONFIG        : 0x"));
  Serial.println(radio.getRegister(CONFIG), HEX);

  Serial.print(F("EN_AA         : 0x"));
  Serial.println(radio.getRegister(EN_AA), HEX);

  Serial.print(F("EN_RX_ADDR    : 0x"));
  Serial.println(radio.getRegister(EN_RXADDR), HEX);

  Serial.print(F("SETUP_AW      : 0x"));
  Serial.println(radio.getRegister(SETUP_AW), HEX);

  Serial.print(F("SETUP_RETR    : 0x"));
  Serial.println(radio.getRegister(SETUP_RETR), HEX);

  Serial.print(F("RF_CH         : 0x"));
  Serial.println(radio.getRegister(RF_CH), HEX);

  Serial.print(F("RF_SETUP      : 0x"));
  Serial.println(radio.getRegister(RF_SETUP), HEX);

  Serial.print(F("STATUS        : 0x"));
  Serial.println(radio.getRegister(STATUS), HEX);

  Serial.print(F("OBSERVE_TX    : 0x"));
  Serial.println(radio.getRegister(OBSERVE_TX), HEX);

  Serial.print(F("RPD           : 0x"));
  Serial.println(radio.getRegister(RPD), HEX);

  // RX_ADDR_P0 (5 bytes)
  radio.read_register(RX_ADDR_P0, buf, 5);
  Serial.print(F("RX_ADDR_P0    : "));
  for (uint8_t i = 0; i < 5; i++) {
    if (buf[i] < 0x10) Serial.print('0');
    Serial.print(buf[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  // RX_ADDR_P1 (5 bytes)
  radio.read_register(RX_ADDR_P1, buf, 5);
  Serial.print(F("RX_ADDR_P1    : "));
  for (uint8_t i = 0; i < 5; i++) {
    if (buf[i] < 0x10) Serial.print('0');
    Serial.print(buf[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  // RX_ADDR_P2–P5 (1 byte each)
  for (uint8_t p = 2; p <= 5; p++) {
    Serial.print(F("RX_ADDR_P"));
    Serial.print(p);
    Serial.print(F("    : 0x"));
    Serial.println(radio.getRegister(RX_ADDR_P0 + p), HEX);
  }

  // TX_ADDR (5 bytes)
  radio.read_register(TX_ADDR, buf, 5);
  Serial.print(F("TX_ADDR       : "));
  for (uint8_t i = 0; i < 5; i++) {
    if (buf[i] < 0x10) Serial.print('0');
    Serial.print(buf[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  // RX payload widths
  for (uint8_t p = 0; p <= 5; p++) {
    Serial.print(F("RX_PW_P"));
    Serial.print(p);
    Serial.print(F("      : "));
    Serial.println(radio.getRegister(RX_PW_P0 + p));
  }

  Serial.print(F("FIFO_STATUS   : 0x"));
  Serial.println(radio.getRegister(FIFO_STATUS), HEX);

  Serial.print(F("DYNPD         : 0x"));
  Serial.println(radio.getRegister(DYNPD), HEX);

  Serial.print(F("FEATURE       : 0x"));
  Serial.println(radio.getRegister(FEATURE), HEX);

  Serial.println(F("=========================================="));
}

void setup() {
  Serial.begin(115200);

  radio.begin();

  // ===== AUTO ACK CONFIG =====
  radio.setAutoAck(false);
  radio.setCRCLength(RF24_CRC_16);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(32);

  radio.setChannel(12);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);

  radio.openReadingPipe(1, address);
  radio.startListening();            // RX mode

  Serial.println("RX AUTO-ACK READY");
}

void loop() {
  if (radio.available()) {
    radio.read(rxBuffer, 32);

    Serial.print("RX: ");
    Serial.println(rxBuffer);
  }
}
