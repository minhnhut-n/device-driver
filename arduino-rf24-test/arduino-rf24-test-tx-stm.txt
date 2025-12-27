#include <SPI.h>
#include <RF24.h>

// CE, CSN
RF24 radio(9, 10);

// MUST MATCH STM32 TX ADDRESS
const byte address[5] = {0x00, 0x00, 0x00, 0x00, 0x02};

char rxBuffer[32];

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!radio.begin()) {
    Serial.println("RF24 not responding");
    while (1);
  }

  // ===== MATCH STM32 CONFIG =====
  radio.setChannel(12);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);

  radio.setAutoAck(false);           // Enable Auto ACK
  radio.disableDynamicPayloads();
  radio.setPayloadSize(32);
  radio.openReadingPipe(0, address);
  radio.disableCRC();
  
  radio.startListening();
  Serial.println("ready to print out!!");
}

void loop() {

  if (radio.available()) {
    uint8_t buf[32];
    radio.read(buf, 32);

    Serial.print("RX RAW: ");
    for (int i = 0; i < 32; i++) {
      Serial.print(buf[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

}
