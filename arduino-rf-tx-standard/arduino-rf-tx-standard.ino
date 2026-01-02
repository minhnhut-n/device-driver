#include <SPI.h>
#include <RF24.h>
#include "printf.h"

// CE, CSN
RF24 radio(9, 10);

// 5-byte address
const byte address[5] = {'N','H','U','T','1'};

char txPayload[32] = "HELLO_NO_ACK_FROM_TX";

void setup() {
  Serial.begin(115200);
  radio.begin();
  printf_begin();
  radio.printPrettyDetails();

  // ===== NO ACK CONFIG =====
  Serial.println("===== NO ACK CONFIG =====");
  radio.setAutoAck(false);          // ❌ Auto ACK OFF
  radio.setRetries(0, 0);           // ❌ No retry
  radio.disableDynamicPayloads();   // ❌ Fixed payload
  radio.setPayloadSize(32);

  radio.setChannel(0x0C);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);

  radio.openWritingPipe(address);
  radio.stopListening();            // TX mode

  radio.printDetails();

  Serial.println("TX NO-ACK READY");
}

void loop() {
  bool ok = radio.write(&txPayload, 32);

  Serial.print("Send status: ");
  Serial.println(ok ? "OK" : "FAIL");

  delay(1000);
}
