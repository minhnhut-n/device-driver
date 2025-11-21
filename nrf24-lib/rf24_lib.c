/*
 * rf24_lib.c
 *
 *  Created on: Nov 20, 2025
 *      Author: Aelius_Nguyen
 */

#include "rf24_lib.h"

#define PIPE_LEN(pipe, addr_len) ((pipe <= RX_PIPE_ADDR_1) ? addr_len : 1)

/* Helper to assert/deassert CSN */
static inline void csn_low(RF24_Handle *rf) {
    HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, GPIO_PIN_RESET);
}
static inline void csn_high(RF24_Handle *rf) {
    HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, GPIO_PIN_SET);
}

/* Helper to pulse CE (not used heavily here but provided) */
static inline void ce_high(RF24_Handle *rf) {
    HAL_GPIO_WritePin(rf->cfg.cePort, rf->cfg.cePin, GPIO_PIN_SET);
}
static inline void ce_low(RF24_Handle *rf) {
    HAL_GPIO_WritePin(rf->cfg.cePort, rf->cfg.cePin, GPIO_PIN_RESET);
}

static void rf24_set_rx_addr(RF24_Handle *rf) {
	uint8_t cmd = W_REG;
	switch (rf->pipe) {
		case RX_PIPE_ADDR_0:
			cmd |= RX_PIPE_ADDR_0;
			break;
		case RX_PIPE_ADDR_1:
			cmd |= RX_PIPE_ADDR_1;
			break;
		case RX_PIPE_ADDR_2:
			cmd |= RX_PIPE_ADDR_2;
			break;
		case RX_PIPE_ADDR_3:
			cmd |= RX_PIPE_ADDR_3;
			break;
		case RX_PIPE_ADDR_4:
			cmd |= RX_PIPE_ADDR_4;
			break;
		case RX_PIPE_ADDR_5:
			cmd |= RX_PIPE_ADDR_5;
			break;
		default:
			break;
	}

	rf24_write_multi_config(rf, cmd, rf->rx_addr, PIPE_LEN(rf->pipe, rf->addr_len));
}

static void rf24_set_tx_addr(RF24_Handle *rf) {
    uint8_t cmd = W_REG | TX_ADDR;
    rf24_write_multi_config(rf, cmd, rf->tx_addr, rf->addr_len);
}

/*
 * Write configuration data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 */
void rf24_write_config(RF24_Handle *rf,  uint8_t reg, uint8_t data) {
	uint8_t cmd = W_REG | (reg & 0x1F); //for ensuring reg not over 5 bits
    uint8_t tx[2] = { cmd, data }; // second byte clocks out register
    uint8_t rx[2] = {0}; // 1 byte left for exit line

	csn_low(rf);
    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, tx, rx, 2, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write config \r\n");
    }
	csn_high(rf);
}
void rf24_write_multi_config(RF24_Handle *rf,  uint8_t reg, uint8_t* data, uint8_t size) {
    if (size == 0) return;
    uint8_t cmd = W_REG | (reg & 0x1F);

    uint8_t status = 0;

	csn_low(rf);
    HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status, 1, RF_SPI_TIMEOUT);
    if (status == 0x00) {
    	printf("[RF24] device-died\r\n");
    }

    for (uint8_t i = 0; i < size; ++i) {
        uint8_t dout = data[i];
        uint8_t din = 0;
        HAL_SPI_TransmitReceive(rf->cfg.hspi, &dout, &din, 1, RF_SPI_TIMEOUT);
    }
	csn_high(rf);
}

/*
 * Write user data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 * write to pay load with multiple data
 */
void rf24_write_data(RF24_Handle *rf, uint8_t* data, uint8_t size) {
	uint8_t cmd = W_PAY_LOAD;

	csn_low(rf);
    uint8_t status;
    HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status, 1, RF_SPI_TIMEOUT);
    if (status == 0x00) {
    	printf("[RF24] device-died\r\n");
    }

    for (uint8_t i = 0; i < size; i++) {
        uint8_t dout = data[i], din;
        HAL_SPI_TransmitReceive(rf->cfg.hspi, &dout, &din, 1, RF_SPI_TIMEOUT);
    }
	csn_high(rf);
}

/*
 * Read configuration data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 * read configuration with 1 byte.
 */
uint8_t rf24_read_config(RF24_Handle *rf, uint8_t reg) {
	uint8_t cmd = R_REG | (reg & 0x1F); //for ensuring reg not over 5 bits

    /*
     * 2 phase: 1 is for first return STATUS, 2 is for actual data
     * for actual data we dump a NOP command (dont care), just for reading
     */
	uint8_t tx[2] = { cmd, NOP };
    uint8_t rx[2] = {0};

	csn_low(rf);
    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, tx, rx, 2, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when read config \r\n");
    }
	csn_high(rf);

	return rx[1];
}

/*
 * Read user data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 * read data with multiple byte.
 */
void rf24_read_data(RF24_Handle *rf, uint8_t* buffer, uint8_t size) {
	uint8_t cmd = R_PAY_LOAD;

    uint8_t status;

	csn_low(rf);
    HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status, 1, RF_SPI_TIMEOUT);
    for (uint8_t i = 0; i < size; i++) {
        uint8_t dout = 0xFF, din;
        HAL_SPI_TransmitReceive(rf->cfg.hspi, &dout, &din, 1, RF_SPI_TIMEOUT);
        buffer[i] = din;
    }
	csn_high(rf);
}

/*
 * Read user configuration RF24
 * data as pointer unit8_t, it can be array[] or single byte
 * read data with multiple byte.
 */
void rf24_read_multi(RF24_Handle *rf, uint8_t reg, uint8_t *buf, uint8_t size)
{
	if (size == 0) return;
	uint8_t cmd = R_REG | (reg & 0x1F); //for ensuring reg not over 5 bits

	//Check byte 1st (status) to know system state
    uint8_t status = 0;

    HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status, 1, RF_SPI_TIMEOUT);
    if (status == 0x00) {
    	printf("[RF24] device-died\r\n");
    }

    while (size--) {
        uint8_t dout = 0xFF; //dump byte
        uint8_t din = 0;
        HAL_SPI_TransmitReceive(rf->cfg.hspi, &dout, &din, 1, RF_SPI_TIMEOUT);
        *buf++ = din;
    }
    HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, GPIO_PIN_SET);
}

/*
 * Read flag for new package data come in
 * Data is not padded, it empty and receive new ones.
 */
bool rf24_isDataReady(RF24_Handle *rf) {
	uint8_t value = rf24_read_config(rf, (uint8_t)STATUS_REG);

	if (value & (1<<6)) {
		return true;
	}

	return false;
}

/*
 * Check size of data is coming
 */
uint8_t rf24_rx_bufSize(RF24_Handle *rf) {
	uint8_t value = 0;
	uint8_t status = 0;
	uint8_t cmd = R_RX_PL_WID;

	csn_low(rf);
    HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status, 1, RF_SPI_TIMEOUT);
    if (status == 0x00) {
    	printf("[RF24] device-died\r\n");
    }
    HAL_SPI_TransmitReceive(rf->cfg.hspi, (uint8_t[]){0xFF}, &value, 1, RF_SPI_TIMEOUT);
	csn_high(rf);

	return value;
}

void rf24_switch_mode(RF24_Handle *rf, uint8_t mode) {
//	rf24_switch_mode(mode); //1: RX, 0: TX
	uint8_t value = rf24_read_config(rf, (uint8_t)CONFIG_REG);

	value = (value & 0xFE) | (mode << 0);

	rf24_write_config(rf, (uint8_t)CONFIG_REG, (uint8_t)value);
}

/*
 * Initial for RF24 transmission with mode (TX/ RX)
 */
void rf24_init(RF24_Handle *rf, uint8_t mode) {
	printf("===============\r\n");
	printf("rf->channel %d\r\n", rf->channel);
	printf("rf->baudrate %d\r\n", rf->baudrate);
	printf("rf->pipe %d\r\n", rf->pipe);
	printf("rf->tx_addr %s\r\n", rf->tx_addr);

	//low when write, high with rf24 listen others.
	ce_low(rf);
	//turn on auto feedback (auto ack), is only for high model, pay attention to it

	//addr wide
	uint8_t addrWidth = rf->addr_len;
    if (addrWidth < 3) addrWidth = 3;
    if (addrWidth > 5) addrWidth = 5;

	uint8_t data = rf24_read_config(rf, SET_ADDR_WID);
	data &= ~0x03; //clear before set

	data |= (addrWidth-2);
	rf24_write_config(rf, SET_ADDR_WID, data);

	//pipe
	if (mode == RX_MODE) rf24_set_rx_addr(rf);
	else	  			 rf24_set_tx_addr(rf);

	//channel
	rf24_write_config(rf, SET_FREQ_CHA, rf->channel);

	//	baudrate
	rf24_write_config(rf, SET_REG_RATE, rf->baudrate);

	//power on
	data = rf24_read_config(rf, (uint8_t)CONFIG_REG);
	data |= (1<<1);
	data = (data & 0xFE) | (mode << 0);
	rf24_write_config(rf, CONFIG_REG, data);
	HAL_Delay(2);

    if(mode == RX_MODE)
        ce_high(rf); //listening in rx mode
}


void rf24_factory_reset(RF24_Handle *rf) {
	// Power down
	rf24_write_config(rf, CONFIG_REG, 0x08);

	rf24_write_config(rf, EN_AUTO_ACK, 0x3F);
	rf24_write_config(rf, EN_RX_ADDR, 0x03);
	rf24_write_config(rf, SET_ADDR_WID, 0x03);
	rf24_write_config(rf, SET_AUTO_RETRS, 0x03);
	rf24_write_config(rf, SET_FREQ_CHA, 0x02);
	rf24_write_config(rf, SET_REG_RATE, 0x0F);

	// Clear interrupts
	rf24_write_config(rf, STATUS_REG, 0x70);

	// FIFO reset
	rf24_write_config(rf, FLUSH_TX, 0);
	rf24_write_config(rf, FLUSH_RX, 0);

	// Default addresses (5 bytes)
	uint8_t addr_p0[5] = {0xE7,0xE7,0xE7,0xE7,0xE7};
	uint8_t addr_p1[5] = {0xC2,0xC2,0xC2,0xC2,0xC2};

	rf24_write_multi_config(rf, W_REG | RX_PIPE_ADDR_0, addr_p0, 5);
	rf24_write_multi_config(rf, W_REG | TX_ADDR,        addr_p0, 5);
	rf24_write_multi_config(rf, W_REG | RX_PIPE_ADDR_1, addr_p1, 5);

	rf24_write_config(rf, RX_PIPE_ADDR_2, 0xC3);
	rf24_write_config(rf, RX_PIPE_ADDR_3, 0xC4);
	rf24_write_config(rf, RX_PIPE_ADDR_4, 0xC5);
	rf24_write_config(rf, RX_PIPE_ADDR_5, 0xC6);

	// Disable all payload widths
	rf24_write_config(rf, RX_PW_P0, 0x00);
	rf24_write_config(rf, RX_PW_P1, 0x00);
	rf24_write_config(rf, RX_PW_P2, 0x00);
	rf24_write_config(rf, RX_PW_P3, 0x00);
	rf24_write_config(rf, RX_PW_P4, 0x00);
	rf24_write_config(rf, RX_PW_P5, 0x00);
}

/*
 * DEBUG FUNCTION
 */
void print_reg(const char *name, uint8_t value)
{
    printf("%s = 0x%02X\r\n", name, value);
}
void print_addr(const char *name, uint8_t *addr, uint8_t len)
{
    printf("%s = ", name);
    for (int i = 0; i < len; i++)
        printf("%02X ", addr[i]);

    printf("\r\n");
}
