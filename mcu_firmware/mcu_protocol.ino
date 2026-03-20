#include <Wire.h>
#include <SPI.h>

#define SYNC_BYTE    0xAA
#define CMD_I2C_READ     0x01
#define CMD_I2C_WRITE    0x02
#define CMD_GPIO_SET     0x03
#define CMD_GPIO_GET     0x04
#define CMD_SPI_TRANSFER 0x05
#define CMD_PING         0x06

#define RESP_OK    0x00
#define RESP_ERROR 0xFF

#define ERR_TIMEOUT    0x01
#define ERR_I2C_NACK   0x02
#define ERR_BAD_CRC    0x03
#define ERR_BAD_CMD    0x04
#define ERR_BAD_LEN    0x05

#define MAX_DATA_LEN 255
#define UART_TIMEOUT_MS 100

static const uint8_t crc8_table[256] = {
  0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83,
  0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41,
  0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e,
  0x5f, 0x01, 0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc,
  0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0,
  0xe1, 0xbf, 0x5d, 0x03, 0x80, 0xde, 0x3c, 0x62,
  0xbe, 0xe0, 0x02, 0x5c, 0xdf, 0x81, 0x63, 0x3d,
  0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff,
  0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5,
  0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59, 0x07,
  0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58,
  0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a,
  0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8, 0xe6,
  0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24,
  0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b,
  0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7, 0xb9,
  0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f,
  0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd,
  0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92,
  0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0x0e, 0x50,
  0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c,
  0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0, 0xee,
  0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d, 0xef, 0xb1,
  0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73,
  0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49,
  0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b,
  0x57, 0x09, 0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4,
  0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16,
  0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a,
  0x2b, 0x75, 0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8,
  0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7,
  0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35
};

uint8_t crc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc = crc8_table[crc ^ data[i]];
  }
  return crc;
}

uint8_t rx_buf[MAX_DATA_LEN + 4];
uint8_t tx_buf[MAX_DATA_LEN + 4];

bool read_bytes(uint8_t *buf, size_t n) {
  size_t received = 0;
  unsigned long start = millis();
  while (received < n) {
    if (millis() - start > UART_TIMEOUT_MS) return false;
    if (Serial.available()) {
      buf[received++] = Serial.read();
    }
  }
  return true;
}

void send_response(uint8_t status, const uint8_t *data, uint8_t len) {
  tx_buf[0] = SYNC_BYTE;
  tx_buf[1] = status;
  tx_buf[2] = len;
  if (data && len > 0) {
    memcpy(&tx_buf[3], data, len);
  }
  tx_buf[3 + len] = crc8(tx_buf, 3 + len);
  Serial.write(tx_buf, 4 + len);
}

void send_error(uint8_t err_code) {
  send_response(RESP_ERROR, &err_code, 1);
}

void handle_i2c_read(uint8_t *data, uint8_t len) {
  if (len < 2) { send_error(ERR_BAD_LEN); return; }
  uint8_t addr = data[0];
  uint8_t nbytes = data[1];
  
  Wire.requestFrom((int)addr, (int)nbytes);
  
  unsigned long start = millis();
  while (Wire.available() < nbytes) {
    if (millis() - start > UART_TIMEOUT_MS) {
      send_error(ERR_TIMEOUT);
      return;
    }
  }
  
  uint8_t resp[nbytes];
  for (int i = 0; i < nbytes; i++) {
    resp[i] = Wire.read();
  }
  send_response(RESP_OK, resp, nbytes);
}

void handle_i2c_write(uint8_t *data, uint8_t len) {
  if (len < 2) { send_error(ERR_BAD_LEN); return; }
  uint8_t addr = data[0];
  
  Wire.beginTransmission(addr);
  Wire.write(&data[1], len - 1);
  uint8_t result = Wire.endTransmission();
  
  if (result == 0) {
    send_response(RESP_OK, nullptr, 0);
  } else {
    send_error(ERR_I2C_NACK);
  }
}
void handle_gpio_set(uint8_t *data, uint8_t len) {
  if (len < 2) { send_error(ERR_BAD_LEN); return; }
  uint8_t pin = data[0];
  uint8_t val = data[1];
  pinMode(pin, OUTPUT);
  digitalWrite(pin, val);
  send_response(RESP_OK, nullptr, 0);
}

void handle_gpio_get(uint8_t *data, uint8_t len) {
  if (len < 1) { send_error(ERR_BAD_LEN); return; }
  uint8_t pin = data[0];
  pinMode(pin, INPUT);
  uint8_t val = digitalRead(pin);
  send_response(RESP_OK, &val, 1);
}

void handle_spi_transfer(uint8_t *data, uint8_t len) {
  if (len < 1) { send_error(ERR_BAD_LEN); return; }
  uint8_t resp[len];
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  for (int i = 0; i < len; i++) {
    resp[i] = SPI.transfer(data[i]);
  }
  SPI.endTransaction();
  send_response(RESP_OK, resp, len);
}

void handle_ping() {
  uint8_t pong = 0x01;
  send_response(RESP_OK, &pong, 1);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  SPI.begin();
}

void loop() {
  if (!Serial.available()) return;
  uint8_t sync = Serial.read();
  if (sync != SYNC_BYTE) return;

  uint8_t header[2];
  if (!read_bytes(header, 2)) { send_error(ERR_TIMEOUT); return; }
  uint8_t cmd = header[0];
  uint8_t len = header[1];

  uint8_t data[MAX_DATA_LEN];
  if (len > 0) {
    if (!read_bytes(data, len)) { send_error(ERR_TIMEOUT); return; }
  }

  uint8_t received_crc;
  if (!read_bytes(&received_crc, 1)) { send_error(ERR_TIMEOUT); return; }

  rx_buf[0] = SYNC_BYTE;
  rx_buf[1] = cmd;
  rx_buf[2] = len;
  memcpy(&rx_buf[3], data, len);
  uint8_t expected_crc = crc8(rx_buf, 3 + len);
  if (received_crc != expected_crc) { send_error(ERR_BAD_CRC); return; }

  switch (cmd) {
    case CMD_I2C_READ:     handle_i2c_read(data, len);     break;
    case CMD_I2C_WRITE:    handle_i2c_write(data, len);    break;
    case CMD_GPIO_SET:     handle_gpio_set(data, len);     break;
    case CMD_GPIO_GET:     handle_gpio_get(data, len);     break;
    case CMD_SPI_TRANSFER: handle_spi_transfer(data, len); break;
    case CMD_PING:         handle_ping();                  break;
    default:               send_error(ERR_BAD_CMD);        break;
  }
}
