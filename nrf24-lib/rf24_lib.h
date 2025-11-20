/*
 * rf24_lib.h
 *
 *  Created on: Nov 20, 2025
 *      Author: Aelius_Nguyen
 */

#include "rf24_info_reg.h"
#include "stm32f1xx_hal.h"
#include "stdbool.h"

#ifndef INC_RF24_LIB_H_
#define INC_RF24_LIB_H_

//PinMode
typedef struct {
	SPI_HandleTypeDef *hspi;

	GPIO_TypeDef *cePort;
	uint8_t cePin;

	GPIO_TypeDef *csnPort;
	uint8_t csnPin;
} RF24_Config;

typedef struct {
	RF24_Config cfg;
	uint8_t pipe;
	uint8_t channel;
	uint8_t addr[ADDR_SIZE];

	uint8_t tx_addr[5];
	uint8_t rx_addr[5];
} RF24_Handle;



/*
 * Write configuration data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 */
void rf24_write_config(RF24_Handle *rf,  uint8_t reg, uint8_t data);
void rf24_write_multi_config(RF24_Handle *rf,  uint8_t reg, uint8_t* data, uint8_t size);

/*
 * Write user data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 * write to pay load with multiple data
 */
void rf24_write_data(RF24_Handle *rf, uint8_t data, uint8_t size);

/*
 * Read configuration data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 * read configuration with 1 byte.
 */
uint8_t rf24_read_config(RF24_Handle *rf, uint8_t reg);

/*
 * Read user data into RF24 for transmission
 * data as pointer unit8_t, it can be array[] or single byte
 * read data with multiple byte.
 */
void rf24_read_data(RF24_Handle *rf, uint8_t* buffer, uint8_t size);

/*
 * Check size of data is coming
 */
uint8_t rf24_rx_bufSize(RF24_Handle *rf);

/*
 * Read flag for new package data come in
 * Data is not padded, it empty and receive new ones.
 */
bool rf24_isDataReady(RF24_Handle *rf);

/*
 * Initial for RF24 transmission with mode (TX/ RX)
 */
void rf24_init(RF24_Handle *rf, uint8_t mode);


#endif /* INC_RF24_LIB_H_ */
