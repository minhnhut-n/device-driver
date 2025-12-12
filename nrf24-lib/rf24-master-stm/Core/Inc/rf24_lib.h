/*
 * rf24_lib.h
 *
 *  Created on: Nov 20, 2025
 *      Author: Aelius_Nguyen
 *
 *  This library have some configuration in RF24.h
 *  About macro define
 */

#include <stdint.h>
#include "rf24_info_reg.h"
#include "stm32f1xx_hal.h"
#include "stdbool.h"
#include <string.h>
#include <stdio.h>

#ifndef INC_RF24_LIB_H_
#define INC_RF24_LIB_H_

//PinMode
typedef struct {
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *cePort;
    uint16_t cePin;
    bool ce_status;

    GPIO_TypeDef *csnPort;
    uint16_t csnPin;

    uint8_t rf24_config_reg;
} RF24_Config;

typedef struct {
    RF24_Config cfg;
    uint8_t channel;
    uint8_t baudrate;
    uint8_t power;
    bool dynamic_pay_load;
    uint8_t payload_size;
    uint8_t pipe_auto_ack;
    /**
     * pipe 0 is for auto ack, change to destination addr (tx) to receive ack
     * that need to restore to it own pipe 0 data.
    */
    uint8_t pipe0_tx_addr[MAX_ADDRESS];
    uint8_t pipe0_rx_addr[MAX_ADDRESS];
    bool is_restore_pipe0_addr;
    bool is_enable_payload_ack;
    bool is_tx_mode;

} RF24_Handle;

/**
 */
typedef enum
{
    RF24_PA_MIN = 0,
    RF24_PA_LOW,
    RF24_PA_HIGH,
    RF24_PA_MAX,
    RF24_PA_ERROR
} rf24_pa_dbm_e;

typedef enum
{
    /** (0) represents 1 Mbps */
    RF24_1MBPS = 0,
    /** (1) represents 2 Mbps */
    RF24_2MBPS,
    /** (2) represents 250 kbps */
    RF24_250KBPS
} rf24_datarate_e;

typedef enum
{
    /** (0) represents no CRC checksum is used */
    RF24_CRC_DISABLED = 0,
    /** (1) represents CRC 8 bit checksum is used */
    RF24_CRC_8,
    /** (2) represents CRC 16 bit checksum is used */
    RF24_CRC_16
} rf24_crclength_e;

typedef enum
{
    /// @brief The FIFO is not full nor empty, but it is occupied with 1 or 2 payloads.
    RF24_FIFO_OCCUPIED,
    /// @brief The FIFO is empty.
    RF24_FIFO_EMPTY,
    /// @brief The FIFO is full.
    RF24_FIFO_FULL,
    /// @brief Represents corruption of data over SPI (when observed).
    RF24_FIFO_INVALID,
} rf24_fifo_state_e;

typedef enum
{
    /// An alias of `0` to describe no IRQ events enabled.
    RF24_IRQ_NONE = 0,
    /// Represents an event where TX Data Failed to send.
    RF24_TX_DF = 1 << MASK_MAX_RT,
    /// Represents an event where TX Data Sent successfully.
    RF24_TX_DS = 1 << TX_DS,
    /// Represents an event where RX Data is Ready to `RF24::read()`.
    RF24_RX_DR = 1 << RX_DR,
    /// Byte to clear all interrupt on previous section
    RF24_IRQ_ALL = (1 << MASK_MAX_RT) | (1 << TX_DS) | (1 << RX_DR),
} rf24_irq_flags_e;


/**
 * @brief helper function for stm32-spi purpose
 * in this function, csn (chip select pin will go low to enable transmission)
 */
void spi_beginTransaction(RF24_Handle *rf);
/**
 * @brief helper function for stm32stm32-spi purpose
 * in this function, csn (chip select pin will go high to disable transmission)
 */
void spi_endTransaction(RF24_Handle *rf);

/**
 * @brief function support for writing configuration wih spi
 */
void rf24_write_reg(RF24_Handle *rf, uint8_t reg, const uint8_t *regData, uint8_t size);
/**
 * @brief function support for reading configuration wih spi
 */
void rf24_read_reg(RF24_Handle *rf, uint8_t reg, uint8_t* buffer, uint8_t size);

/**
 * @brief function support for writing user data wih spi
 */
void rf24_write_data(RF24_Handle *rf, const uint8_t* buffer, uint8_t size, uint8_t writeType);

/**
 * @brief function support for reading user data wih spi
 */
void rf24_read_data(RF24_Handle *rf, uint8_t* buffer, uint8_t size);

/**
 * @brief Using SETUP_AW to check it is value or not.
 * Value on this REG can be 1,2,3 respectively with 3,4,5 bytes
 * So the offset is = -2
 */
bool isValid_AddrWidth(RF24_Handle *rf);

/**
 * @brief Power consumption for rf24
 */
void rf24_power_set(RF24_Handle *rf);

/**
 * @brief change pin CE logic and status
 */
void rf24_ce_pin(RF24_Handle *rf, bool status);
/**
 * Open pipe data for reading
 */
void rf24_pipeData_rx_open(RF24_Handle *rf, uint8_t pipeNum, const uint8_t* addressRX);
/**
 * Close pipe data for reading
 */
void rf24_pipeData_rx_close(RF24_Handle *rf, uint8_t pipeNum);
/**
 * Registry for TX tunnel prepare for writing
 */
void rf24_pipeData_tx_registry(RF24_Handle *rf, const uint8_t* address);

/**
 * @brief Configuration mode RX on RF24
 */
void rf24_rx_mode(RF24_Handle *rf);
/**
 * @brief Configuration mode TX on RF24
 */
void rf24_tx_mode(RF24_Handle *rf);
/**
 * @brief Configuration mode STANDBY on RF24
 */
void rf24_standby_mode(RF24_Handle *rf);

/**
 * @brief Start mode listening on RF24
 * @def when listening start, mode turn from TX -> RX mode
 * then device could you rf24_read to read value from rf24
 */
void rf24_listen_start(RF24_Handle *rf);
/**
 * @brief Stop mode listening on RF24
 * @def this function mean, when listening is done, close 
 * section and return to default (tx mode)
 */
void rf24_listen_stop(RF24_Handle *rf);

/**
 * @brief Empty buffer TX
 */
void rf24_empty_tx_buffer(RF24_Handle *rf);
/**
 * @brief Empty buffer RX
 */
void rf24_empty_rx_buffer(RF24_Handle *rf);


/**
 * @brief Power consumption for rf24
 */
void rf24_powerConsumption_set(RF24_Handle *rf);
/**
 * @brief Channel set for rf24
 */
void rf24_channel_set(RF24_Handle *rf, uint8_t channel);
/**
 * @brief Baudrate set for rf24
 */
void rf24_baudrate_set(RF24_Handle *rf, uint8_t baudrate);

/**
 * @brief Init RF24 module
 */
uint8_t rf24_init(RF24_Handle *rf);


/**
 * FOR DEBUG WITH SERIAL LOG ONLY
 */

void print_state_init(RF24_Handle *rf, uint8_t pipeNum);
void print_tc_function(RF24_Handle *rf, uint8_t pipeNum);
void print_reg(const char *name, uint8_t value);
void print_addr(const char *name, uint8_t *addr, uint8_t len);

#endif /* INC_RF24_LIB_H_ */
