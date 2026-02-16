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

#define RF_SPI_TIMEOUT 200
#define MAX_ADDRESS 5

#ifndef INC_RF24_LIB_H_
#define INC_RF24_LIB_H_

//GPIO and SPI physical pin config
typedef struct {
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *cePort;
    uint16_t cePin;
    bool ce_status;

    GPIO_TypeDef *csnPort;
    uint16_t csnPin;

    uint8_t rf24_config_reg;
} RF24_Config;

//struct to store rf24 state
typedef struct {
    RF24_Config cfg;
    uint8_t payload_size;
    /**
     * pipe 0 is for auto ack, change to destination addr (tx) to receive ack
     * that need to restore to it own pipe 0 data.
    */
    uint8_t addr_len;
    uint8_t tx_addr[MAX_ADDRESS];
    uint8_t pipe0_rx_addr[MAX_ADDRESS];
    uint8_t pipe0_tx_addr[MAX_ADDRESS];
    uint8_t crc_setting;
    uint8_t cmd_send_data;
    bool is_restore_pipe0_addr;
    bool is_auto_ack;
    bool is_pipe0_rx;
    bool is_p_variant;

    //should be set as default -> disable, and enable by function
    bool dynamic_payload_enabled;
    bool power_state;
} RF24_Handle;

//carrier-wave strenght settings
typedef enum
{
    RF24_PA_MIN = 0,
    RF24_PA_LOW,
    RF24_PA_HIGH,
    RF24_PA_MAX,
    RF24_PA_ERROR
} rf24_pa_dbm_e;

// data rate in the air
typedef enum
{
    RF24_1MBPS = 0,
    RF24_2MBPS,
    RF24_250KBPS
} rf24_datarate_e;

//crc type
typedef enum
{
    RF24_CRC_DISABLED = 0,
    RF24_CRC_8,
    RF24_CRC_16
} rf24_crclength_e;

//fifo enums check status
typedef enum
{
    RF24_FIFO_OCCUPIED,
    RF24_FIFO_EMPTY,
    RF24_FIFO_FULL,
    RF24_FIFO_INVALID,
} rf24_fifo_state_e;

// value for setting register value
typedef enum
{
    RF24_IRQ_NONE = 0,
    RF24_TX_DF = 1 << MASK_MAX_RT,
    RF24_TX_DS = 1 << TX_DS,
    RF24_RX_DR = 1 << RX_DR,
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
void rf24_write_reg(RF24_Handle *rf, uint8_t reg, uint8_t regData);
/**
 * @brief function support for writing configuration wih spi
 */
void rf24_write_reg_mul(RF24_Handle *rf, uint8_t reg, const uint8_t* regData, uint8_t size);

/**
 * @brief function support for reading configuration wih spi
 */
void rf24_read_reg(RF24_Handle *rf, uint8_t reg, uint8_t* buffer);
/**
 * @brief function support for reading configuration wih spi
 */
void rf24_read_reg_mul(RF24_Handle *rf, uint8_t reg, uint8_t* buffer, uint8_t size);

/**
 * @brief: init rf24 module
 * @note:
 */
void rf24_init(RF24_Handle *rf);

/**
 * @brief frame of sending step
 */
uint8_t rf24_transmit(RF24_Handle *rf, const uint8_t* buffer, uint8_t size);
/**
 * @brief function support for writing user data wih spi
 */
uint8_t rf24_write_data(RF24_Handle *rf, const uint8_t* buffer, uint8_t size);

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
void rf24_sig_amp(RF24_Handle *rf);

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
 * @brief Check data available or not
 */
bool rf24_is_dataAvailable(RF24_Handle *rf, uint8_t pipeNum);

/**
 * @brief Configuration mode RX on RF24
 */
void rf24_rx_mode(RF24_Handle *rf, uint8_t pipeNum, uint8_t* addressRX);
/**
 * @brief Configuration mode TX on RF24
 */
void rf24_tx_mode(RF24_Handle *rf, const uint8_t* tx_address);
/**
 * @brief Configuration mode STANDBY on RF24
 */
void rf24_standby_mode(RF24_Handle *rf);

/**
 * @brief Flush buffer TX
 */
void rf24_flush_tx_buffer(RF24_Handle *rf);
/**
 * @brief Flush buffer RX
 */
void rf24_flush_rx_buffer(RF24_Handle *rf);
/**
 * @brief Power set for rf24
 */
void rf24_power_enable_set(RF24_Handle *rf, uint8_t status);
/**
 * @brief Power consumption for rf24
 */
void rf24_PA_set(RF24_Handle *rf, uint8_t level);
/**
 * @brief Channel set for rf24
 */
void rf24_channel_set(RF24_Handle *rf, uint8_t channel);
/**
 * @brief Baudrate set for rf24
 */
void rf24_baudrate_set(RF24_Handle *rf, uint8_t baudrate);
/**
 * @brief Auto Acknowledgment configuration for rf24
 */
void rf24_autoAck_enable(RF24_Handle *rf, bool type);
void rf24_autoAck_config(RF24_Handle *rf, uint16_t ack_time, uint8_t ack_retry);
void rf24_ack_payload(RF24_Handle *rf, bool is_ack_payload);
void rf24_dynamic_payLoad(RF24_Handle *rf, bool type);
void rf24_cmd_on_write(RF24_Handle *rf, bool write_with_ack);
void rf24_carrier_wave_enable(RF24_Handle *rf, bool enable);
void rf24_pll_lock_enable(RF24_Handle *rf, bool enable);
/**
 * @brief: CRC setting for payload transmit
 */
void rf24_crc_setting(RF24_Handle *rf, bool state, uint8_t numCRCByte);

/**
 * @brief: reset rf24 module
 * @note:
 */
void rf24_reset(RF24_Handle *rf);

/**
 * FOR DEBUG WITH SERIAL LOG ONLY
 */

void print_state_init(RF24_Handle *rf, uint8_t pipeNum);
void print_tc_function(RF24_Handle *rf, uint8_t pipeNum);
void print_reg(const char *name, uint8_t value);
void print_addr(const char *name, uint8_t *addr, uint8_t len);
void rf24_dump_registers(RF24_Handle *rf);


#endif /* INC_RF24_LIB_H_ */