#include <SPI.h>
#include <RF24.h>

// CE, CSN
RF24 radio(9, 10);

const byte address[5] = {'N','H','U','T','1'};
char rxBuffer[32];

void setup() {
  Serial.begin(115200);
  radio.begin();

  // ===== NO ACK CONFIG =====
  radio.setAutoAck(false);
  radio.disableDynamicPayloads();
  radio.setCRCLength(RF24_CRC_16);
  radio.setPayloadSize(32);

  radio.setChannel(12);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);

  radio.openReadingPipe(0, address);
  radio.startListening();           // RX mode

  Serial.println("RX NO-ACK READY");
}

void loop() {
  if (radio.available()) {
    radio.read(&rxBuffer, 32);

    Serial.print("RX: ");
    Serial.println(rxBuffer);
  }
}
