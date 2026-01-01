#include <SPI.h>
#include <RF24.h>
#include "printf.h"


// CE, CSN
RF24 radio(9, 10);

// MUST MATCH RX
const byte address[5] = {'N','H','U','T','1'};

uint8_t txBuffer[32];
uint8_t counter = 0;

void dumpRF24Registers()
{
  Serial.println(F("===== RF24 REGISTER DUMP ====="));
  radio.printDetails();
  Serial.println(F("===== END DUMP ====="));
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!radio.begin()) {
    Serial.println("RF24 not responding");
    while (1);
  }

  printf_begin();

  // ===== RF CONFIG =====
  radio.setChannel(12);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MIN);

  radio.setAutoAck(true);      // ENABLE Auto ACK
  radio.setRetries(5, 15);     // Retry for ACK
  radio.disableDynamicPayloads();
  radio.setPayloadSize(32);
  radio.setCRCLength(RF24_CRC_16);

  radio.openWritingPipe(address);
  radio.stopListening();

  dumpRF24Registers();

  Serial.println("TX ready (Auto ACK ON)");
}

void loop() {
  memset(txBuffer, 0, sizeof(txBuffer));

  // Example payload
  txBuffer[0] = counter++;
  txBuffer[1] = 'N';
  txBuffer[2] = 'H';
  txBuffer[3] = 'U';
  txBuffer[4] = 'T';

  bool ok = radio.write(txBuffer, 32);

  Serial.print("TX result: ");
  Serial.println(ok ? "ACK OK" : "MAX_RT (NO ACK)");

  delay(500);
}
