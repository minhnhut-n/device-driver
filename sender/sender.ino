#include <SPI.h>
#include <RF24.h>

// CE, CSN
RF24 radio(9, 10);

// 5-byte address (TX == RX)
const byte address[5] = {'N','H','U','T','1'};
char txPayload[32] = "NhutNguyen0000000000000000000000";

uint8_t nrf_read_reg(uint8_t reg)
{
  digitalWrite(10, LOW);
  SPI.transfer(0x00 | (reg & 0x1F)); // R_REGISTER
  uint8_t val = SPI.transfer(0xFF);
  digitalWrite(10, HIGH);
  return val;
}

void nrf_read_buf(uint8_t reg, uint8_t *buf, uint8_t len)
{
  digitalWrite(10, LOW);
  SPI.transfer(0x00 | (reg & 0x1F));
  for (uint8_t i = 0; i < len; i++)
    buf[i] = SPI.transfer(0xFF);
  digitalWrite(10, HIGH);
}

void dump_nrf24()
{
  uint8_t buf[5];

  Serial.println("========== nRF24L01 REGISTER DUMP ==========");

  Serial.print("CONFIG        : 0x"); Serial.println(nrf_read_reg(0x00), HEX);
  Serial.print("EN_AA         : 0x"); Serial.println(nrf_read_reg(0x01), HEX);
  Serial.print("EN_RX_ADDR    : 0x"); Serial.println(nrf_read_reg(0x02), HEX);
  Serial.print("SETUP_AW      : 0x"); Serial.println(nrf_read_reg(0x03), HEX);
  Serial.print("SETUP_RETR    : 0x"); Serial.println(nrf_read_reg(0x04), HEX);
  Serial.print("RF_CH         : 0x"); Serial.println(nrf_read_reg(0x05), HEX);
  Serial.print("RF_SETUP      : 0x"); Serial.println(nrf_read_reg(0x06), HEX);
  Serial.print("STATUS        : 0x"); Serial.println(nrf_read_reg(0x07), HEX);
  Serial.print("OBSERVE_TX    : 0x"); Serial.println(nrf_read_reg(0x08), HEX);
  Serial.print("RPD           : 0x"); Serial.println(nrf_read_reg(0x09), HEX);

  nrf_read_buf(0x0A, buf, 5);
  Serial.print("RX_ADDR_P0    : ");
  for (int i = 0; i < 5; i++) { Serial.print(buf[i], HEX); Serial.print(" "); }
  Serial.println();

  nrf_read_buf(0x0B, buf, 5);
  Serial.print("RX_ADDR_P1    : ");
  for (int i = 0; i < 5; i++) { Serial.print(buf[i], HEX); Serial.print(" "); }
  Serial.println();

  Serial.print("RX_ADDR_P2    : 0x"); Serial.println(nrf_read_reg(0x0C), HEX);
  Serial.print("RX_ADDR_P3    : 0x"); Serial.println(nrf_read_reg(0x0D), HEX);
  Serial.print("RX_ADDR_P4    : 0x"); Serial.println(nrf_read_reg(0x0E), HEX);
  Serial.print("RX_ADDR_P5    : 0x"); Serial.println(nrf_read_reg(0x0F), HEX);

  nrf_read_buf(0x10, buf, 5);
  Serial.print("TX_ADDR       : ");
  for (int i = 0; i < 5; i++) { Serial.print(buf[i], HEX); Serial.print(" "); }
  Serial.println();

  Serial.print("RX_PW_P0      : "); Serial.println(nrf_read_reg(0x11));
  Serial.print("RX_PW_P1      : "); Serial.println(nrf_read_reg(0x12));
  Serial.print("RX_PW_P2      : "); Serial.println(nrf_read_reg(0x13));
  Serial.print("RX_PW_P3      : "); Serial.println(nrf_read_reg(0x14));
  Serial.print("RX_PW_P4      : "); Serial.println(nrf_read_reg(0x15));
  Serial.print("RX_PW_P5      : "); Serial.println(nrf_read_reg(0x16));

  Serial.print("FIFO_STATUS   : 0x"); Serial.println(nrf_read_reg(0x17), HEX);
  Serial.print("DYNPD         : 0x"); Serial.println(nrf_read_reg(0x1C), HEX);
  Serial.print("FEATURE       : 0x"); Serial.println(nrf_read_reg(0x1D), HEX);

  Serial.println("==========================================");
}

void setup() {
  Serial.begin(115200);

  radio.begin();

  // ===== AUTO ACK CONFIG =====
  radio.setAutoAck(false);
  // radio.setRetries(5, 15);
  radio.setCRCLength(RF24_CRC_16);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(32);

  radio.setChannel(12);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_MAX);

  radio.openWritingPipe(address);
  radio.stopListening();             // TX mode

   dump_nrf24();
  Serial.println("TX AUTO-ACK READY");
}

void loop() {
  bool ok = radio.write(txPayload, 32);

  Serial.print("Send status: ");
  Serial.println(ok ? "ACK OK" : "ACK FAIL");

  delay(1000);
}
