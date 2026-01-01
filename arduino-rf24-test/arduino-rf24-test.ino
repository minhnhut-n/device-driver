#include <SPI.h>
#include <RF24.h>

// CE, CSN
RF24 radio(9, 10);

// MUST MATCH TX
const byte address[5] = {'N','H','U','T','1'};

char rxBuffer[32];
bool ledState = false;

void setup() {
  pinMode(8, OUTPUT);
  Serial.begin(115200);
  while (!Serial);

  if (!radio.begin()) {
    Serial.println("RF24 not responding");
    while (1);
  }

  // ===== RF CONFIG =====
  radio.setChannel(12);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MIN);

  radio.setAutoAck(false);        // ENABLE Auto ACK
  // radio.setAutoAck(1, true);     // Pipe 1
  radio.disableDynamicPayloads();
  radio.setPayloadSize(32);
  radio.setCRCLength(RF24_CRC_16);

  radio.openReadingPipe(1, address);
  radio.startListening();

  Serial.println("RX ready (Auto ACK ON)");
}

void loop() {
  if (radio.available()) {
    radio.read(rxBuffer, 32);

    ledState = !ledState;
    digitalWrite(8, ledState);

    Serial.print("RX RAW: ");
    for (int i = 0; i < 32; i++) {
      Serial.print(rxBuffer[i]);
      Serial.print(" ");
    }
    Serial.println();
  }
}
