/*
 * rf24_lib.c
 *
 *  Created on: Nov 20, 2025
 *      Author: Aelius_Nguyen
 */

#include "rf24_lib.h"
#include "stm_system_config.h"

/**
 * =============================================================================
 * Macro
 * Check bool value
 */
#define IS_EMPTY_BUFFER(buf)      ((buf) != NULL ? 1 : 0)
#define MINVALUE(val1, val2)	((val1) < (val2) ? (val1) : (val2))
#define STATUS_ON_CHECK(cmd) (((cmd) == W_TX_PAYLOAD) ? (V_TX_DS | V_MAX_RT) : (((cmd) == W_TX_PAYLOAD_NOACK) ? V_TX_DS : 0))
#define TX_MAX_RETRIES 2
#define TX_RETRY_TIMEOUT 150

static const uint8_t pipeAddr[6] = {RX_PIPE_ADDR_0, RX_PIPE_ADDR_1, RX_PIPE_ADDR_2,
                                    RX_PIPE_ADDR_3, RX_PIPE_ADDR_4, RX_PIPE_ADDR_5};

/**
 * Static function
 * This function will be use only on internal of this file,
 * No export API outside
 * 
 * =============================================================================
 */

/**
 * @version 0.1
 * @brief rf24_clear_irq
 * @author minhnhut-n
 * @function: used to clear interrupt flag after transmission, such as, MAX_RT, RX_DR, TX_DS
 */
static inline void rf24_clear_irq(RF24_Handle *rf)
{
    uint8_t clr = (V_MAX_RT | V_RX_DR | V_TX_DS);
    rf24_write_reg(rf, STATUS_REG, clr);
}

/**
 * @version 0.1
 * @brief convert_us_to_tick
 * @author minhnhut-n
 * @function: some delay need to base on microseconds, this function 
 * help convert us to tick count needed to set delay
 */
static inline uint32_t convert_us_to_tick(uint32_t us) {
    return us*(SystemCoreClock/1e6);
}

/**
 * @version 0.1
 * @brief convert_tick_to_us
 * @author minhnhut-n
 * @function: this is reverse function of convert_us_to_tick
 */
static inline uint32_t convert_tick_to_us(uint32_t tick) {
    return tick/(SystemCoreClock/1e6);
}

// /**
//  * @version 0.1
//  * @brief software_reset
//  * @author minhnhut-n
//  * @function: used to reset whole system, using nvic reset,
//  * this function is now simulate reset button on STM board
//  */
// static void software_reset(void)
// {
//     __disable_irq();        // optional nhưng nên có
//     NVIC_SystemReset();     // reset toàn bộ hệ thống
// }

/**
 * @version 0.1
 * @brief spi_beginTransaction
 * @author minhnhut-n
 * @function: pull-down csn-pin to start spi transaction
 */
void spi_beginTransaction(RF24_Handle *rf)
{
    HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, GPIO_PIN_RESET);
}

/**
 * @version 0.1
 * @brief spi_endTransaction
 * @author minhnhut-n
 * @function: pull-up CSN pin, spi transaction end
 */
void spi_endTransaction(RF24_Handle *rf)
{
    HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, GPIO_PIN_SET);
}

/**
 * @version 0.1
 * @brief rf24_write_reg
 * @author minhnhut-n
 * @function: support write single byte to register (type 1)
 */
void rf24_write_reg(RF24_Handle *rf, uint8_t reg, uint8_t regData)
{
    rf24_ce_pin(rf, false);
    spi_beginTransaction(rf);
    uint8_t status, dump;
    uint8_t cmd = W_REG | (reg & 0x1F); //for ensuring reg not over 5 bits

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write config %02X \r\n", reg);
    }

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &regData, &dump, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write config data %02X \r\n", reg);
    }
    spi_endTransaction(rf);
}

/**
 * @version 0.1
 * @brief rf24_write_reg_mul
 * @author minhnhut-n
 * @function: support to write multiple byte to register (type 2)
 */
void rf24_write_reg_mul(RF24_Handle *rf, uint8_t reg, const uint8_t* regData, uint8_t size)
{
    rf24_ce_pin(rf, false);
    spi_beginTransaction(rf);
    uint8_t status, dump[MAX_ADDRESS];
    uint8_t cmd = W_REG | (reg & 0x1F); //for ensuring reg not over 5 bits

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write config %02X \r\n", reg);
    }

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, regData, dump, size, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write config data %02X \r\n", reg);
    }
    spi_endTransaction(rf);
}

/**
 * @version 0.1
 * @brief rf24_read_reg
 * @author minhnhut-n
 * @function: support to read register data from nrf24, there is no seperate API such as the "write" ones.
 */
void rf24_read_reg(RF24_Handle *rf, uint8_t reg, uint8_t* buffer, uint8_t size)
{
    spi_beginTransaction(rf);
    uint8_t cmd = R_REG | (reg & 0x1F); //for ensuring reg not over 5 bits
    uint8_t dummy = NOP;
    uint8_t status;

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when read config %02X \r\n", reg);
    }

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &dummy, buffer, size, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when read config data %02X \r\n", reg);
    }
    spi_endTransaction(rf);   
}

/**
 * @version 0.1
 * @brief rf24_whatHappened
 * @author minhnhut-n
 * @function: base on status data (1 byte) parameter, this check and return current status of nrf24
 * @behavior:
 * - return true (success)
 * - return fail (something wrong that can not transmit data)
 */
typedef enum {
    RF24_EVT_FAIL = 0,
    RF24_EVT_TX_OK = 1,
    RF24_EVT_MAX_RT = 2,
    RF24_EVT_RX_DR = 3,
    RF24_EVT_TIMEOUT = 4
} rf24_event_t;

/**
 * @brief Analyze STATUS register and return event code; if timed_out==true
 * perform diagnostics and recovery steps.
 * @param rf handle
 * @param status STATUS register value
 * @param timed_out whether caller detected a wait timeout
 * @return rf24_event_t event code
 */
static rf24_event_t rf24_whatHappened(RF24_Handle *rf, uint8_t status, bool timed_out) {
    printf("[DBG] rf24_whatHappened: STATUS=0x%02X timed_out=%d\r\n", status, timed_out);

    if (status & V_MAX_RT) {
        printf("[ERR] TX max retries!\r\n");
        return RF24_EVT_MAX_RT;
    }

    if (status & V_TX_DS) {
        printf("[INFO] TX is success!\r\n");
        return RF24_EVT_TX_OK;
    }

    if (status & V_RX_DR) {
        printf("[INFO] Having data in RX!\r\n");
        return RF24_EVT_RX_DR;
    }

    if (timed_out) {
        // timeout diagnostics + recovery (moved from rf24_transmit)
        printf("[ERR] Transmit timeout\r\n");
        uint8_t obs = 0;
        rf24_read_reg(rf, OBSERVE_TX, &obs, ONE_BYTE);
        printf("[DBG] OBSERVE_TX=0x%02X\r\n", obs);
        uint8_t cfg = 0;
        rf24_read_reg(rf, CONFIG_REG, &cfg, ONE_BYTE);
        printf("[DBG] CONFIG=0x%02X (PRIM_RX=%d, PWR_UP=%d)\r\n", cfg, (cfg>>PRIM_RX)&1, (cfg>>PWR_UP)&1);
        uint8_t fifo = 0;
        rf24_read_reg(rf, FIFO_STATUS, &fifo, ONE_BYTE);
        printf("[DBG] FIFO_STATUS=0x%02X\r\n", fifo);
        uint8_t txaddr[5] = {0};
        rf24_read_reg(rf, TX_ADDR, txaddr, MAX_ADDRESS);
        printf("[DBG] TX_ADDR = "); for (int i=0;i<MAX_ADDRESS;i++) printf("%02X", txaddr[i]); printf("\r\n");
        uint8_t rx0[5] = {0};
        rf24_read_reg(rf, RX_PIPE_ADDR_0, rx0, MAX_ADDRESS);
        printf("[DBG] RX_ADDR_P0 = "); for (int i=0;i<MAX_ADDRESS;i++) printf("%02X", rx0[i]); printf("\r\n");
        printf("[DBG] cmd_send_data=0x%02X, is_auto_ack=%d, is_tx_mode=%d, ce_status=%d\r\n",
               rf->cmd_send_data, rf->is_auto_ack, rf->is_tx_mode, rf->cfg.ce_status);

        rf24_clear_irq(rf);
        rf24_empty_tx_buffer(rf);
        rf24_empty_rx_buffer(rf);

        // Recovery: power cycle the nRF24 to recover from stuck state
        printf("[WARN] Performing nRF24 recovery (power cycle + reset)...\r\n");
        rf24_power_enable_set(rf, false);
        HAL_Delay(50);
        rf24_reset(rf);
        rf24_power_enable_set(rf, true);
        rf24_tx_mode(rf, rf->tx_addr);
        HAL_Delay(2);

        return RF24_EVT_TIMEOUT;
    }

    return RF24_EVT_FAIL;
}

/**
 * @version 0.1
 * @brief rf24_transmit
 * @author minhnhut-n
 * @function: user-api to send the data
 */
uint8_t rf24_transmit(RF24_Handle *rf, const uint8_t* buffer, uint8_t size)
{
    printf("Transmit data: ");
    for (int i = 0; i < size; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\r\n");

    rf24_tx_mode(rf, rf->tx_addr);

    uint8_t write_ok = rf24_write_data(rf, buffer, size);
    if (!write_ok) {
        rf24_clear_irq(rf);
        rf24_empty_tx_buffer(rf);
        rf24_empty_rx_buffer(rf);
        return 0; // write failed
    }

    uint8_t status = 0;
    uint32_t start = HAL_GetTick();
    const uint32_t timeout = 500; // milliseconds (extended for diagnostics)
    uint32_t last_print = start;

    /* Raise CE to start transmission and keep it high while waiting */
    rf24_ce_pin(rf, true);
    delay_us(20);

    // Diagnostic: print immediate status and FIFO after writing payload
    uint8_t immediate_status = 0;
    rf24_read_reg(rf, STATUS_REG, &immediate_status, ONE_BYTE);
    uint8_t immediate_fifo = 0;
    rf24_read_reg(rf, FIFO_STATUS, &immediate_fifo, ONE_BYTE);
    uint8_t feat = 0;
    rf24_read_reg(rf, FEATURE, &feat, ONE_BYTE);
    uint8_t en_aa = 0;
    rf24_read_reg(rf, EN_AA, &en_aa, ONE_BYTE);
    printf("[DBG] post-write STATUS=0x%02X, FIFO=0x%02X, FEATURE=0x%02X, EN_AA=0x%02X\r\n",
           immediate_status, immediate_fifo, feat, en_aa);

    /* Read OBSERVE_TX to get ARC and PLOS counters for diagnostics */
    uint8_t observe = 0;
    rf24_read_reg(rf, OBSERVE_TX, &observe, ONE_BYTE);
    uint8_t arc_cnt = observe & 0x0F;
    uint8_t plos_cnt = (observe >> 4) & 0x0F;
    printf("[DBG] OBSERVE_TX=0x%02X (ARC=%u, PLOS=%u)\r\n", observe, arc_cnt, plos_cnt);

    do {
        rf24_read_reg(rf, STATUS_REG, &status, ONE_BYTE);
        if ((HAL_GetTick() - last_print) >= 50) {
            printf("[DBG] wait status=0x%02X\r\n", status);
            last_print = HAL_GetTick();
        }
    } while (!(status & STATUS_ON_CHECK(rf->cmd_send_data)) && (HAL_GetTick() - start < timeout));

    // Check why we exited the wait loop
    bool timed_out = !(status & STATUS_ON_CHECK(rf->cmd_send_data));

    if (timed_out) {
        uint32_t elapsed = HAL_GetTick() - start;
        printf("[DBG] transmit wait elapsed=%lu ms, final STATUS=0x%02X, expected_mask=0x%02X\r\n",
               (unsigned long)elapsed, status, (unsigned int)STATUS_ON_CHECK(rf->cmd_send_data));
    }

    // Clear CE now that we finished waiting (success, max-rt or timeout)
    rf24_ce_pin(rf, false);

    // Retry logic: if timeout, attempt retry before recovery
    if (timed_out) {
        for (int retry_attempt = 0; retry_attempt < TX_MAX_RETRIES; retry_attempt++) {
            printf("[DBG] TX retry attempt %d/%d\r\n", retry_attempt + 1, TX_MAX_RETRIES);

            /* Clean up and re-attempt transmission */
            rf24_clear_irq(rf);
            rf24_empty_tx_buffer(rf);
            HAL_Delay(10);

            /* Re-write payload */
            write_ok = rf24_write_data(rf, buffer, size);
            if (!write_ok) {
                printf("[DBG] TX retry: write_data failed\r\n");
                continue;
            }

            /* Read diagnostics after re-write */
            rf24_read_reg(rf, STATUS_REG, &immediate_status, ONE_BYTE);
            rf24_read_reg(rf, FIFO_STATUS, &immediate_fifo, ONE_BYTE);
            rf24_read_reg(rf, OBSERVE_TX, &observe, ONE_BYTE);
            printf("[DBG] retry post-write STATUS=0x%02X, FIFO=0x%02X, OBSERVE_TX=0x%02X\r\n",
                   immediate_status, immediate_fifo, observe);

            /* Raise CE and wait with shorter timeout for retry */
            rf24_ce_pin(rf, true);
            delay_us(20);

            start = HAL_GetTick();
            status = 0;

            /* Wait for TX completion with shorter retry timeout */
            do {
                rf24_read_reg(rf, STATUS_REG, &status, ONE_BYTE);
            } while (!(status & STATUS_ON_CHECK(rf->cmd_send_data)) &&
                    (HAL_GetTick() - start < TX_RETRY_TIMEOUT));

            rf24_ce_pin(rf, false);

            timed_out = !(status & STATUS_ON_CHECK(rf->cmd_send_data));
            uint32_t retry_elapsed = HAL_GetTick() - start;
            printf("[DBG] retry wait elapsed=%lu ms, STATUS=0x%02X, success=%d\r\n",
                   (unsigned long)retry_elapsed, status, !timed_out);

            if (!timed_out) {
                printf("[DBG] TX retry successful!\r\n");
                break; // Retry succeeded, exit retry loop
            }
        }
    }

    // Delegate status handling and timed-out diagnostics/recovery to rf24_whatHappened
    rf24_event_t evt = rf24_whatHappened(rf, status, timed_out);

    rf24_clear_irq(rf);
    rf24_empty_tx_buffer(rf);
    rf24_empty_rx_buffer(rf);

    if (timed_out && evt == RF24_EVT_TIMEOUT) {
        return 2; // timeout
    }

    if (evt == RF24_EVT_TX_OK || evt == RF24_EVT_RX_DR) {
        return 1;
    } else if (evt == RF24_EVT_MAX_RT) {
        return 3;
    }

    return 0;
}

/**
 * @version 0.1
 * @brief rf24_write_data
 * @author minhnhut-n
 * @function: child process to write data into register
 */
uint8_t rf24_write_data(RF24_Handle *rf, const uint8_t* buffer, uint8_t size)
{
    //pre-processsing data
	uint8_t payloadTX[ONE_SECTION_BUF] = {0};
    if (rf->dynamic_pay_load) {
        if (rf->payload_size == 0) {
            printf("[ERR] Wrong payload size setting!!\r\n");
            return 0;
        }
        size = MINVALUE(size, ONE_SECTION_BUF);
    	memcpy(payloadTX, buffer, size);
    } else {
        memset(payloadTX, 0, ONE_SECTION_BUF);
    	memcpy(payloadTX, buffer, size);
        size = ONE_SECTION_BUF;
    }


    uint8_t status_reg, dump;
    //condition on write data
    rf24_ce_pin(rf, false);
    //spi transmission set
    spi_beginTransaction(rf);
    uint8_t cmd = rf->cmd_send_data;

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status_reg, ONE_BYTE, SPI_TIMEOUT) != HAL_OK) {
        printf("[ERR] write fail: %02X \r\n", cmd);
        spi_endTransaction(rf);
        return 0;
    }

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, payloadTX, &dump, size, SPI_TIMEOUT) != HAL_OK) {
        printf("[ERR] data sending\r\n");
        spi_endTransaction(rf);
        return 0;
    }
    spi_endTransaction(rf);

    // Do not toggle CE here. Let caller (rf24_transmit) control CE so it can
    // remain high while waiting for TX_DS/MAX_RT. This prevents premature
    // lowering of CE which may reduce successful transmissions.
    return 1;
}

/**
 * @version 0.1
 * @brief rf24_read_data
 * @author minhnhut-n
 * @function: used to read buffer from nrf24 with multiple bytes.
 */
void rf24_read_data(RF24_Handle *rf, uint8_t* buffer, uint8_t size)
{
    //dynamic payload check
    if (rf->dynamic_pay_load) {
        size = MINVALUE(size, ONE_SECTION_BUF);
    }
    else {
        size = ONE_SECTION_BUF;
    }

    uint8_t status_reg;
    rf24_ce_pin(rf, false);
    spi_beginTransaction(rf);
    uint8_t cmd = R_PAY_LOAD;
    uint8_t dummy = NOP;
    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status_reg, ONE_BYTE, SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &dummy, buffer, size, SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }
    spi_endTransaction(rf);
}

/**
 * @version 0.1
 * @brief rf24_is_dataAvailable
 * @author minhnhut-n
 * @function: if data available flag will be set to notify register have data
 */
bool rf24_is_dataAvailable(RF24_Handle *rf, uint8_t pipeNum)
{
    uint8_t status = 0;
    uint8_t rx_p_no = 0;
    rf24_read_reg(rf, STATUS_REG, &status, ONE_BYTE);
    rx_p_no = (status >> RX_P_NO) & 0x07;

    if ((status & (1 << RX_DR)) && (rx_p_no == pipeNum)) {  
        return true;
    }
    return false;
}

/**
 * @version 0.1
 * @brief isValid_AddrWidth
 * @author minhnhut-n
 * @function: check address width is valid or not (3-5 bytes)
 */
bool isValid_AddrWidth(RF24_Handle *rf)
{
    rf24_ce_pin(rf, false);    
    uint8_t rtn = 0;
    rf24_read_reg(rf, SETUP_AW, &rtn, ONE_BYTE);

    rtn += ADDR_WD_OFFSET;
    if (rtn > 2 && rtn < 6) {
        // Address size is correct
        return true;
    }
    return false;
}

/**
 * @version 0.1
 * @brief rf24_PA_set
 * @author minhnhut-n
 * @function: power consumtion setting level from -18dBm to 0dBm
 * which is devided into 4 levels: low - mid - high - max
 */
void rf24_PA_set(RF24_Handle *rf, uint8_t level)
{
    rf24_ce_pin(rf, false);
    uint8_t config = 0;
    rf24_read_reg(rf, RF_SETUP, &config, ONE_BYTE);

    config &= ~((1<<1) | (1<<2));
    config |= ((level&0x03) << 1) | 0x01;

    rf24_write_reg(rf, RF_SETUP, config);
}

/**
 * @version 0.1
 * @brief rf24_channel_set
 * @author minhnhut-n
 * @function: channel configuration
 */
void rf24_channel_set(RF24_Handle *rf, uint8_t channel)
{
    rf24_ce_pin(rf, false);
    uint8_t reset = 0;
    rf24_read_reg(rf, RF_CH, &reset, ONE_BYTE);

    if(channel > 125 || channel < 0)
    {
        printf("Channel set invalid\r\n");
        return;
    }
    rf24_write_reg(rf, RF_CH, channel);
}

/**
 * @version 0.1
 * @brief rf24_baudrate_set
 * @author minhnhut-n
 * @function: setting data rate on air range from 250kbps to 2Mbps
 */
void rf24_baudrate_set(RF24_Handle *rf, uint8_t baudrate)
{
    rf24_ce_pin(rf, false);
    uint8_t config = 0;
    rf24_read_reg(rf, RF_SETUP, &config, ONE_BYTE);

    switch (baudrate)
    {
    case BAUD_1MBPS:
        config &= ~(1<<RF_DR_HIGH);
        config &= ~(1<<RF_DR_LOW);
        break;
    case BAUD_2MBPS:
        config |= (1<<RF_DR_HIGH);
        config &= ~(1<<RF_DR_LOW);
        break;
    case BAUD_250KB:
        config &= ~(1<<RF_DR_HIGH);
        config |= (1<<RF_DR_LOW);
        break;
    default:
        break;
    }
    rf24_write_reg(rf, RF_SETUP, config);
}

/**
 * @version 0.1
 * @brief rf24_power_enable_set
 * @author minhnhut-n
 * @function: power nrf24 on/off, allway the first gate before any operation
 */
void rf24_power_enable_set(RF24_Handle *rf, uint8_t status)
{
    uint8_t config = 0;
    rf24_ce_pin(rf, false);
    rf24_read_reg(rf, CONFIG_REG, &config, ONE_BYTE);

    if (status) {
        config |= (1 << PWR_UP);
        rf->power_state = true;
    }
    else {
        config &= ~(1 << PWR_UP);
        rf->power_state = false;
    }

    rf24_write_reg(rf, CONFIG_REG, config);

    if (status) {
        HAL_Delay(2);  // Wait for power-up (1.5ms minimum)
    }
}

/**
 * @version 0.1
 * @brief rf24_power_amp_set
 * @author minhnhut-n
 * @function: set the strength of power amplifier from -18dBm to 0dBm
 */
void rf24_power_amp_set(RF24_Handle *rf, uint8_t level) {
    uint8_t rf_setup;
    rf24_ce_pin(rf, false);

    rf24_read_reg(rf, RF_SETUP, &rf_setup, ONE_BYTE);
    //clear all bit before set
    rf_setup &= ~(3 << 1);
    rf_setup |= (level << 1);
    rf24_write_reg(rf, RF_SETUP, rf_setup);
}

/**
 * @version 0.1
 * @brief rf24_crc_setting
 * @author minhnhut-n
 * @function: redundant check on the entirety of data, can enable/disable and set number of bytes
 * default is 2 bytes CRC on ARDUINO library
 */
void rf24_crc_setting(RF24_Handle *rf, bool enable, uint8_t numCRCByte)
{
    rf24_ce_pin(rf, false);

    rf->crc_setting = numCRCByte;

    uint8_t config = 0;
    rf24_read_reg(rf, CONFIG_REG, &config, ONE_BYTE);
    if ( enable ) {
        config |= (1 << EN_CRC);
        if (numCRCByte == 1) {
            config &= ~(1 << CRCO);
        }
        else
        {
            config |= (1 << CRCO);
        }
    }
    else {
        config &= ~(1 << EN_CRC);
    }
    rf24_write_reg(rf, CONFIG_REG, config);
}

/**
 * @version 0.1
 * @brief rf24_ce_pin
 * @author minhnhut-n
 * @function: set ce pin logic high/low, and update to handle variable status.
 */
void rf24_ce_pin(RF24_Handle *rf, bool status)
{
    HAL_GPIO_WritePin(rf->cfg.cePort, rf->cfg.cePin, status ? GPIO_PIN_SET : GPIO_PIN_RESET);
    rf->cfg.ce_status = status;
}

/**
 * @version 0.1
 * @brief rf24_pipeData_rx_open
 * @author minhnhut-n
 * @function: for receiver application, open specific pipe to receive data
 */
void rf24_pipeData_rx_open(RF24_Handle *rf, uint8_t pipeNum, const uint8_t* addressRX)
{
    rf24_ce_pin(rf, false);    
    if (pipeNum == PIPE0) {
        // recover address on pipe 0 (RX)
        memcpy(rf->pipe0_rx_addr, addressRX, MAX_ADDRESS);
        rf->is_restore_pipe0_addr = true;
    }

    //Due to PIPE 2->5, share 4 bytes address with PIPE1 (same)
    //It must be edit 1 LSB for address.
    //PIPE0 share address with TX_PIPE,
    //It must be not overwrite on TX_PIPE_ADDR when it is in TX mode
    uint8_t targetPipeAddr = pipeAddr[pipeNum];
    uint8_t value = 0;

    if (pipeNum <= 5) {
        //from pipe2 to pipe 5
        if (pipeNum >= 2) {
            //copy 4 others first byte to PIPE1 address
            uint8_t currAddr[MAX_ADDRESS] = {0};
            memcpy(currAddr, addressRX, MAX_ADDRESS);
            uint8_t addr = currAddr[MAX_ADDRESS-1];
            currAddr[MAX_ADDRESS-1] = 0;
            rf24_read_reg(rf, RX_PIPE_ADDR_1, currAddr, MAX_ADDRESS);
            rf24_write_reg(rf, targetPipeAddr, addr);
        }
        else if (pipeNum == PIPE1 || !(rf->is_tx_mode)) {
            rf24_write_reg_mul(rf, targetPipeAddr, addressRX, MAX_ADDRESS);
        }
    }

    // Set payload width for the pipe
    uint8_t pw_reg = RX_PW_P0 + pipeNum;
    if (rf->dynamic_pay_load) {
        value = rf->payload_size; // Dynamic payload size
    } else {
        value = ONE_SECTION_BUF; // Static payload size
    }
    rf24_write_reg(rf, pw_reg, value);

    rf24_read_reg(rf, EN_RXADDR, &value, ONE_BYTE);
    value |= (ENABLE << pipeNum);
    rf24_write_reg(rf, EN_RXADDR, value);
}

/**
 * @version 0.1
 * @brief rf24_pipeData_rx_close
 * @author minhnhut-n
 * @function: for receiver application, close specific pipe to stop receiving data
 * (use in case of unwanted data on specific pipe)
 */
void rf24_pipeData_rx_close(RF24_Handle *rf, uint8_t pipeNum)
{
    rf24_ce_pin(rf, false);    
    uint8_t value = 0;
    rf24_read_reg(rf, EN_RXADDR, &value, ONE_BYTE);
    value &= ~(ENABLE << pipeNum);
    rf24_write_reg(rf, EN_RXADDR, value);

    if (pipeNum == 0) {
        // keep track of pipe 0's RX state to avoid null vs 0 in addr cache
        rf->is_restore_pipe0_addr = false;
    }
}

/**
 * @version 0.1
 * @brief rf24_autoAck_enable
 * @author minhnhut-n
 * @function: auto-ack for 2 ways verifing and sending data (high reliable) 
 */
void rf24_autoAck_enable(RF24_Handle *rf, bool type)
{
    rf24_ce_pin(rf, false);    
    uint8_t config_aa = 0x00;

    if (type)
    {
        config_aa = 0x3F; //enable all pipe
        rf->is_auto_ack = true;
    }
    else
    {
        config_aa = 0x00; //disable all pipe
        rf->is_auto_ack = false;
    }
    rf24_write_reg(rf, EN_AA, config_aa);
}

/**
 * @version 0
 * @brief rf24_autoAck_config
 * @author minhnhut-n
 * @function: configuration for autoack if it is enabled
 * setting about 2 parameters:
 * ack_time: time, which is a gap between 2 consecutive send
 * ack_retry: number of times resend (no ack is received)
 */
void rf24_autoAck_config(RF24_Handle *rf, uint16_t ack_time, uint8_t ack_retry)
{
    rf24_ce_pin(rf, false);    
    if (ack_time < 250) {
        printf("rate is not valid, set to default\r\n");
        ack_time = 250;
    }

    uint8_t config = 0x00;  
    config |= ((uint8_t)(ack_time / 250) - 1) << 4 | (ack_retry << 0);
    rf24_write_reg(rf, SETUP_RETR, config);
}

/**
 * @version 0
 * @brief rf24_ack_payload
 * @author minhnhut-n
 * @function: ack payload enable/disable, allow receiver send back another message
 * beside ack signal.
 * default if no ack payload, it only receive ack signal (1 byte).
 */
void rf24_ack_payload(RF24_Handle *rf, bool is_ack_payload)
{
    rf24_ce_pin(rf, false);
    uint8_t feature = 0x00;
    rf24_read_reg(rf, FEATURE, &feature, ONE_BYTE);

    if (is_ack_payload) {
        feature |= (1 << EN_ACK_PAY);
    }
    else {
        feature &= ~(1 << EN_ACK_PAY);
    }

    rf24_write_reg(rf, FEATURE, feature);
}

/**
 * @version 0.1
 * @brief rf24_dynamic_payLoad
 * @author minhnhut-n
 * @function: allow to send flexible payload frame from 1 to 32 bytes data
 * (only available on nrf24l01+)
 */
void rf24_dynamic_payLoad(RF24_Handle *rf, bool type) {
    rf24_ce_pin(rf, false);
    uint8_t feature = 0x00;
    rf24_read_reg(rf, FEATURE, &feature, ONE_BYTE);

    if (type) {
        feature |= (1 << EN_DPL);
        rf->dynamic_pay_load = true;
        rf24_write_reg(rf, DYNPD, 0x3F); // enable dynamic payload trên tất cả pipe
    } else {
        feature &= ~(1 << EN_DPL);
        rf->dynamic_pay_load = false;
        rf24_write_reg(rf, DYNPD, 0x00); // disable dynamic payload
    }
    rf24_write_reg(rf, FEATURE, feature);
}

/**
 * @version 0
 * @brief rf24_addr_width_set
 * @author minhnhut-n
 * @function: wide of address frame from 3 to 5 bytes
 */
void rf24_addr_width_set(RF24_Handle *rf, uint8_t size)
{
    rf24_ce_pin(rf, false);
    size &= 0x03;
    rf24_write_reg(rf, SETUP_AW, size);
}

/**
 * @version 0
 * @brief rf24_cmd_on_write
 * @author minhnhut-n
 * @function: used to setting on payload with ack_payload or not
 */
void rf24_cmd_on_write(RF24_Handle *rf, bool write_with_ack)
{
    rf24_ce_pin(rf, false);

    uint8_t feature = 0x00;
    rf24_read_reg(rf, FEATURE, &feature, ONE_BYTE);
    if (feature == 0x00) {
        spi_beginTransaction(rf);
        uint8_t cmd[2] = {0x50, 0x73}; // ACTIVATE sequence
        uint8_t status;
        HAL_SPI_TransmitReceive(rf->cfg.hspi, cmd, &status, 2, SPI_TIMEOUT);
        spi_endTransaction(rf);
        rf24_read_reg(rf, FEATURE, &feature, ONE_BYTE);
    }

    rf24_read_reg(rf, FEATURE, &feature, ONE_BYTE);
    if (write_with_ack) {
        rf->cmd_send_data = W_TX_PAYLOAD;
        feature &= ~(1 << EN_DYN_ACK);
    }
    else {
        rf->cmd_send_data = W_TX_PAYLOAD_NOACK;
        feature |= (1 << EN_DYN_ACK);
    }
    rf24_write_reg(rf, FEATURE, feature);
}

/**
 * @version 0
 * @brief rf24_rx_mode
 * @author minhnhut-n
 * @function
 */
void rf24_rx_mode(RF24_Handle *rf, uint8_t pipeNum, uint8_t* addressRX)
{
    printf("RX Mode");

    rf->is_tx_mode = false;
    rf24_ce_pin(rf, false);

    //enable address pipe
    rf24_pipeData_rx_open(rf, pipeNum, addressRX);
    rf24_read_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    rf->cfg.rf24_config_reg |= (RX_MODE << PRIM_RX);
    if (  !(rf->cfg.rf24_config_reg & (1 << PWR_UP)) ) {
        rf->cfg.rf24_config_reg |= (1 << PWR_UP);
    }

    rf24_write_reg(rf, CONFIG_REG, rf->cfg.rf24_config_reg);
    rf24_ce_pin(rf, true);
}

/**
 * @version 0
 * @brief rf24_tx_addr_setting
 * @author minhnhut-n
 * @function: set address before transmittion (tx side)
 */
static void rf24_tx_addr_setting(RF24_Handle *rf, const uint8_t* tx_address)
{
    rf->is_tx_mode = true;
    rf24_ce_pin(rf, false);
    memcpy(rf->tx_addr, tx_address, MAX_ADDRESS);

    //write tx address
    rf24_write_reg_mul(rf, TX_ADDR, rf->tx_addr, MAX_ADDRESS);
    //write ack address (pipe0), only allow write in rx mode
    rf24_write_reg_mul(rf, RX_PIPE_ADDR_0, rf->tx_addr, MAX_ADDRESS);

    if (rf->is_auto_ack) {
        rf24_autoAck_config(rf, RE_ACK_TIME, RE_ACK_COUNT);
    }
}

/**
 * @version 0
 * @brief rf24_tx_mode
 * @author minhnhut-n
 * @function: used in switching mode from RX to TX
 */
void rf24_tx_mode(RF24_Handle *rf, const uint8_t* tx_address)
{
    rf->is_tx_mode = true;
    rf24_tx_addr_setting(rf, tx_address);

    // Ensure powered and set TX mode
    rf24_power_enable_set(rf, true);
    uint8_t config = 0;
    rf24_read_reg(rf, CONFIG_REG, &config, ONE_BYTE);
    config &= ~(1 << PRIM_RX);
    rf24_write_reg(rf, CONFIG_REG, config);

    if (rf->is_auto_ack)
        rf24_crc_setting(rf, true, rf->crc_setting);

    HAL_Delay(2);
    rf24_empty_tx_buffer(rf);
    rf24_empty_rx_buffer(rf);
}

/**
 * @version 0.1
 * @brief rf24_standby_mode
 * @author minhnhut-n
 * @function: switch mode into standby mode (in case need power saving or idle state)
 */
void rf24_standby_mode(RF24_Handle *rf)
{
    rf24_ce_pin(rf, false);
    rf24_read_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    if ( !(rf->cfg.rf24_config_reg & (1 << PWR_UP)) ) {
        rf->cfg.rf24_config_reg |= (1 << PWR_UP);
        rf24_write_reg(rf, CONFIG_REG, rf->cfg.rf24_config_reg);
    }

    rf->is_tx_mode = false;
    HAL_Delay(1);
}

/**
 * @version 0
 * @brief rf24_empty_tx_buffer
 * @author minhnhut-n
 * @function: flush TX buffer
 */
void rf24_empty_tx_buffer(RF24_Handle *rf)
{
    rf24_ce_pin(rf, false);    
    spi_beginTransaction(rf);
    uint8_t command = FLUSH_TX;
    uint8_t status;
    HAL_SPI_TransmitReceive(rf->cfg.hspi, &command, &status, ONE_BYTE, SPI_TIMEOUT);
    spi_endTransaction(rf);
}

/**
 * @version 0
 * @brief rf24_empty_rx_buffer
 * @author minhnhut-n
 * @function: flush RX buffer
 */
void rf24_empty_rx_buffer(RF24_Handle *rf)
{
    rf24_ce_pin(rf, false);    
    spi_beginTransaction(rf);
    uint8_t command = FLUSH_RX;
    uint8_t status;
    HAL_SPI_TransmitReceive(rf->cfg.hspi, &command, &status, ONE_BYTE, SPI_TIMEOUT);
    spi_endTransaction(rf);
}

/**
 * @version 0
 * @brief rf24_init
 * @author minhnhut-n
 * @function: all we know, that is init
 */
void rf24_init(RF24_Handle *rf)
{
    printf("\n==== INIT RF24 ====\r\n");

    rf24_ce_pin(rf, false);
    spi_endTransaction(rf);
    delay_us(20);

    rf24_reset(rf);

    rf->payload_size = 32; // set payload size cố định
    rf24_autoAck_enable(rf, true); // bật auto-ack
    rf24_autoAck_config(rf, 1500, 15);
    rf24_crc_setting(rf, true, RF24_CRC_16);
    rf24_addr_width_set(rf, ADDR_5_BYTE);
    rf24_baudrate_set(rf, BAUD_1MBPS);
    rf24_power_amp_set(rf, MAX_POWER);
    rf24_dynamic_payLoad(rf, false); // static payload

    rf24_power_enable_set(rf, true);
    // default send command (use ACK payload by default)
    rf->cmd_send_data = W_TX_PAYLOAD;
    printf("Status CE pin: %d\r\n", rf->cfg.ce_status);
    printf("==== END INIT RF24 ====\r\n");
}

/**
 * @version 0
 * @brief rf24_carrier_wave_enable
 * @author minhnhut-n
 * @function: enable/disable continuous carrier wave output
 */
void rf24_carrier_wave_enable(RF24_Handle *rf, bool enable) {
    rf24_ce_pin(rf, false);
    uint8_t rf_setup;
    rf24_read_reg(rf, RF_SETUP, &rf_setup, ONE_BYTE);

    if (enable) {
        rf_setup |= (1 << CONT_WAVE);
    } else {
        rf_setup &= ~(1 << CONT_WAVE);
    }
    rf24_write_reg(rf, RF_SETUP, rf_setup);
}

/**
 * @version 0
 * @brief rf24_pll_lock_enable
 * @author minhnhut-n
 * @function: enable/disable PLL lock for carrier wave stability
 */
void rf24_pll_lock_enable(RF24_Handle *rf, bool enable) {
    rf24_ce_pin(rf, false);
    uint8_t rf_setup;
    rf24_read_reg(rf, RF_SETUP, &rf_setup, ONE_BYTE);

    if (enable) {
        rf_setup |= (1 << PLL_LOCK);
    } else {
        rf_setup &= ~(1 << PLL_LOCK);
    }
    rf24_write_reg(rf, RF_SETUP, rf_setup);
}

/**
 * @version 0
 * @brief rf24_reset
 * @author minhnhut-n
 * @function
 */
void rf24_reset(RF24_Handle *rf) {

    //pin reset
    printf("rf24_reset: ce and csn\r\n");
    rf24_ce_pin(rf, false);
    spi_endTransaction(rf);

    //register
    printf("rf24_reset: register\r\n");
    rf24_write_reg(rf, CONFIG_REG,   0x08);
    rf24_write_reg(rf, EN_AA,        0x3F);
    rf24_write_reg(rf, EN_RXADDR,    0x03);
    rf24_write_reg(rf, SETUP_AW,     0x03);
    rf24_write_reg(rf, SETUP_RETR,	 0x03);
    rf24_write_reg(rf, RF_CH,        0x02);
    rf24_write_reg(rf, RF_SETUP,     0x07);

    rf24_write_reg(rf, RX_PW_P0, 0x00);
    rf24_write_reg(rf, RX_PW_P1, 0x00);
    rf24_write_reg(rf, RX_PW_P2, 0x00);
    rf24_write_reg(rf, RX_PW_P3, 0x00);
    rf24_write_reg(rf, RX_PW_P4, 0x00);
    rf24_write_reg(rf, RX_PW_P5, 0x00);

    rf24_write_reg(rf, FIFO_STATUS, 0x11);
    rf24_write_reg(rf, DYNPD,       0x00);
    rf24_write_reg(rf, FEATURE,     0x00);

    //empty fifo buffer
    printf("rf24_reset: empty buffer\r\n");
    rf24_empty_rx_buffer(rf);
    rf24_empty_tx_buffer(rf);
}

/**
 * =============================================================================
 * FOR DEBUG WITH SERIAL LOG ONLY
 * =============================================================================
 */

/**
 * @version 0
 * @brief print_tc_function
 * @author minhnhut-n
 * @function: debug
 */
void print_tc_function(RF24_Handle *rf, uint8_t pipeNum)
{
    printf("\n==>> Test function <<==\r\n");
    uint8_t check = 0;
    rf24_read_reg(rf, CONFIG_REG, &check, ONE_BYTE);
    printf("Value: %02X\r\n", check);

    printf("==>> AutoACK mode <<==\r\n");
    check = 0;
    rf24_read_reg(rf, EN_AA, &check, ONE_BYTE);
    printf("Value: %02X\r\n", check);

    printf("==>> Address config <<==\r\n");
    check = 0;
    rf24_read_reg(rf, SETUP_AW, &check, ONE_BYTE);
    printf("Value: %02X\r\n", check);

    printf("==>> Channel config <<==\r\n");
    check = 0;
    rf24_read_reg(rf, RF_CH, &check, ONE_BYTE);
    printf("Value: %02X\r\n", check);

    printf("==>> Baudrate config <<==\r\n");
    check = 0;
    rf24_read_reg(rf, RF_SETUP, &check, ONE_BYTE);
    printf("Value: %02X\r\n", check);

    printf("==>> Pipe Address	<<==\r\n");
    uint8_t buff[MAX_ADDRESS];
    uint8_t pipeChose = pipeAddr[pipeNum];
    rf24_read_reg(rf, pipeChose, buff, MAX_ADDRESS);
    for (int i = 0; i <  MAX_ADDRESS; i++)
        printf("Value: %02X\r\n", buff[i]);
}

/**
 * @version 0
 * @brief print_state_init
 * @author minhnhut-n
 * @function: debug init
 */
void print_state_init(RF24_Handle *rf, uint8_t pipeNum)
{
    printf("\n==>> Standby mode <<==\r\n");
    uint8_t check = 0;
    rf24_read_reg(rf, CONFIG_REG, &check, ONE_BYTE);
    printf("Value: %02X\r\n", check);

    printf("==>> AutoACK mode <<==\r\n");
    check = 0;
    rf24_read_reg(rf, EN_AA, &check, ONE_BYTE);
    printf("Value: %02X\r\n", check);

    printf("==>> Address config <<==\r\n");
    check = 0;
    rf24_read_reg(rf, SETUP_AW, &check, ONE_BYTE);
    printf("Value: %02X\r\n", check);

    printf("==>> Channel config <<==\r\n");
    check = 0;
    rf24_read_reg(rf, RF_CH, &check, ONE_BYTE);
    printf("Value: %02X\r\n", check);

    printf("==>> Baudrate config <<==\r\n");
    check = 0;
    rf24_read_reg(rf, RF_SETUP, &check, ONE_BYTE);
    printf("Value: %02X\r\n", check);

    printf("==>> Pipe Address	<<==\r\n");
    uint8_t buff[MAX_ADDRESS];
    uint8_t pipeChose = pipeAddr[pipeNum];
    rf24_read_reg(rf, pipeChose, buff, MAX_ADDRESS);
    for (int i = 0; i <  MAX_ADDRESS; i++)
        printf("Value: %02X\r\n", buff[i]);
}

/**
 * @version 0
 * @brief rf24_dump_registers
 * @author minhnhut-n
 * @function: debug
 */
void rf24_dump_registers(RF24_Handle *rf)
{
    uint8_t v;
    uint8_t buf[5];

    printf("\r\n========== nRF24L01 REGISTER DUMP ==========\r\n");

    rf24_read_reg(rf, CONFIG_REG, &v, 1);
    printf("CONFIG        : 0x%02X\r\n", v);

    rf24_read_reg(rf, EN_AA, &v, 1);
    printf("EN_AA         : 0x%02X\r\n", v);

    rf24_read_reg(rf, EN_RXADDR, &v, 1);
    printf("EN_RX_ADDR    : 0x%02X\r\n", v);

    rf24_read_reg(rf, SETUP_AW, &v, 1);
    printf("SETUP_AW      : 0x%02X\r\n", v);

    rf24_read_reg(rf, SETUP_RETR, &v, 1);
    printf("SETUP_RETR    : 0x%02X\r\n", v);

    rf24_read_reg(rf, RF_CH, &v, 1);
    printf("RF_CH         : 0x%02X\r\n", v);

    rf24_read_reg(rf, RF_SETUP, &v, 1);
    printf("RF_SETUP      : 0x%02X\r\n", v);

    rf24_read_reg(rf, STATUS_REG, &v, 1);
    printf("STATUS        : 0x%02X\r\n", v);

    rf24_read_reg(rf, OBSERVE_TX, &v, 1);
    printf("OBSERVE_TX    : 0x%02X\r\n", v);

    rf24_read_reg(rf, RPD, &v, 1);
    printf("RPD           : 0x%02X\r\n", v);

    rf24_read_reg(rf, RX_PIPE_ADDR_0, buf, 5);
    printf("RX_ADDR_P0    : ");
    for (int i = 0; i < 5; i++) printf("%02X ", buf[i]);
    printf("\r\n");

    rf24_read_reg(rf, RX_PIPE_ADDR_1, buf, 5);
    printf("RX_ADDR_P1    : ");
    for (int i = 0; i < 5; i++) printf("%02X ", buf[i]);
    printf("\r\n");

    rf24_read_reg(rf, RX_PIPE_ADDR_2, &v, 1);
    printf("RX_ADDR_P2    : 0x%02X\r\n", v);

    rf24_read_reg(rf, RX_PIPE_ADDR_3, &v, 1);
    printf("RX_ADDR_P3    : 0x%02X\r\n", v);

    rf24_read_reg(rf, RX_PIPE_ADDR_4, &v, 1);
    printf("RX_ADDR_P4    : 0x%02X\r\n", v);

    rf24_read_reg(rf, RX_PIPE_ADDR_5, &v, 1);
    printf("RX_ADDR_P5    : 0x%02X\r\n", v);

    rf24_read_reg(rf, TX_ADDR, buf, 5);
    printf("TX_ADDR       : ");
    for (int i = 0; i < 5; i++) printf("%02X ", buf[i]);
    printf("\r\n");

    rf24_read_reg(rf, RX_PW_P0, &v, 1);
    printf("RX_PW_P0      : %d\r\n", v);

    rf24_read_reg(rf, RX_PW_P1, &v, 1);
    printf("RX_PW_P1      : %d\r\n", v);

    rf24_read_reg(rf, RX_PW_P2, &v, 1);
    printf("RX_PW_P2      : %d\r\n", v);

    rf24_read_reg(rf, RX_PW_P3, &v, 1);
    printf("RX_PW_P3      : %d\r\n", v);

    rf24_read_reg(rf, RX_PW_P4, &v, 1);
    printf("RX_PW_P4      : %d\r\n", v);

    rf24_read_reg(rf, RX_PW_P5, &v, 1);
    printf("RX_PW_P5      : %d\r\n", v);

    rf24_read_reg(rf, FIFO_STATUS, &v, 1);
    printf("FIFO_STATUS   : 0x%02X\r\n", v);

    rf24_read_reg(rf, DYNPD, &v, 1);
    printf("DYNPD         : 0x%02X\r\n", v);

    rf24_read_reg(rf, FEATURE, &v, 1);
    printf("FEATURE       : 0x%02X\r\n", v);

    printf("===========================================\r\n");
}

/**
 * @version 0
 * @brief print_reg
 * @author minhnhut-n
 * @function: debug
 */
void print_reg(const char *name, uint8_t value)
{
    printf("%s = 0x%02X\r\n", name, value);
}

/**
 * @version 0
 * @brief print_addr
 * @author minhnhut-n
 * @function: debug
 */
void print_addr(const char *name, uint8_t *addr, uint8_t len)
{
    printf("%s = ", name);
    for (int i = 0; i < len; i++)
        printf("%02X ", addr[i]);

    printf("\r\n");
}