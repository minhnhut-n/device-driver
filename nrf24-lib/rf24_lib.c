/*
 * rf24_lib.c
 *
 *  Created on: Nov 20, 2025
 *      Author: Aelius_Nguyen
 */

#include "rf24_lib.h"
/*
 * Write configuration data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 */
void rf24_write_config(RF24_Handle *rf,  uint8_t reg, uint8_t data) {
	uint8_t cmd = W_REG | reg;

	HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, 0);
	HAL_SPI_Transmit(rf->cfg.hspi, &cmd, 1, 1);
	HAL_SPI_Transmit(rf->cfg.hspi, &data, 1, 1);
	HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, 1);
}
void rf24_write_multi_config(RF24_Handle *rf,  uint8_t reg, uint8_t* data, uint8_t size) {
	uint8_t cfg = W_REG | reg;

	HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, 0);
	HAL_SPI_Transmit(rf->cfg.hspi, &cfg, 1, 1);
	HAL_SPI_Transmit(rf->cfg.hspi, data, size, 10);
	HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, 1);
}

/*
 * Write user data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 * write to pay load with multiple data
 */
void rf24_write_data(RF24_Handle *rf, uint8_t* data, uint8_t size) {
	uint8_t cfg = W_REG | W_PAY_LOAD;

	HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, 0);
	HAL_SPI_Transmit(rf->cfg.hspi, &cfg, 1, 1);
	HAL_SPI_Transmit(rf->cfg.hspi, data, size, 10);
	HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, 1);
}

/*
 * Read configuration data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 * read configuration with 1 byte.
 */
uint8_t rf24_read_config(RF24_Handle *rf, uint8_t reg) {
	uint8_t cmd = R_REG | reg;
	uint8_t rtn = 0;

	HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, 0);
	HAL_SPI_Transmit(rf->cfg.hspi, &cmd, 1, 1);
	HAL_SPI_Receive(rf->cfg.hspi, &rtn, 1, 1);
	HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, 1);

	return rtn;
}

/*
 * Read user data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 * read data with multiple byte.
 */
void rf24_read_data(RF24_Handle *rf, uint8_t* buffer, uint8_t size) {
	uint8_t cfg = R_REG | R_PAY_LOAD;

	HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, 0);
	HAL_SPI_Transmit(rf->cfg.hspi, &cfg, 1, 1);
	HAL_SPI_Receive(rf->cfg.hspi, buffer, size, 10);
	HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, 1);
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
	uint8_t value =	rf24_read_config(rf, (uint8_t)R_RX_PL_WID);
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
	uint8_t cmd = W_REG;
	uint8_t data = 0;

	//turn on auto feedback (auto ack), is only for high model, pay attention to it

	//addr wide
	cmd = W_REG;
	cmd |= SET_ADDR_WID;
	data = rf24_read_config(rf, (uint8_t)SET_ADDR_WID);
	switch (ADDR_SIZE) {
		case 3:
			data |= 0x01; //3bytes = "vna";
			break;
		case 4:
			data |= 0x10;
			break;
		case 5:
			data |= 0x11;
			break;
		default:
			break;
	}
	rf24_write_config(rf, cmd, data);

	//pipe
	cmd = W_REG;
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
	rf24_write_multi_config(rf, cmd, rf->addr, ADDR_SIZE);

	//channel
	cmd = W_REG;
	cmd |= SET_FREQ_CHA;
	data = 9;
	rf24_write_config(rf, cmd, data);

	//	baudrate
	cmd = W_REG;
	cmd |= SET_REG_RATE;
	data = 0x06;
	rf24_write_config(rf, cmd, data);

	//power on
	cmd = W_REG;
	cmd |= CONFIG_REG;
	data = rf24_read_config(rf, (uint8_t)CONFIG_REG);
	data |= (1<<1);
	data = (data & 0xFE) | (mode << 0);
	rf24_write_config(rf, cmd, data);
	HAL_Delay(2);

}
