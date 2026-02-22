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
#define V_TX_DS             (1 << TX_DS)
#define V_MAX_RT            (1 << MAX_RT)
#define V_RX_DR             (1 << RX_DR)
#define RE_ACK_TIME         4000
#define RE_ACK_COUNT        5
#define STATUS_ON_CHECK(cmd) (((cmd) == W_PAY_LOAD) ? (V_TX_DS | V_MAX_RT) : (((cmd) == W_TX_PAYLOAD_NOACK) ? V_TX_DS : 0))
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
static inline void rf24_clear_all_irq(RF24_Handle *rf)
{
    rf24_write_reg(rf, STATUS_REG, RF24_IRQ_ALL);
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

/**
 * @version 0.1
 * @brief software_reset
 * @author minhnhut-n
 * @function: used to reset whole system, using nvic reset,
 * this function is now simulate reset button on STM board
 */
static void software_reset(void)
{
    __disable_irq();        // optional nhưng nên có
    NVIC_SystemReset();     // reset toàn bộ hệ thống
}

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
    return rf24_write_reg_mul(rf, reg, &regData, 1);
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

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status, 1, RF_SPI_TIMEOUT) != HAL_OK) {
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
 * @function: support to read single register data from nrf24.
 */
int rf24_read_reg(RF24_Handle *rf, uint8_t reg)
{
    uint8_t buffer = 0;
    rf24_read_reg_mul(rf, reg, &buffer, 1);
    return buffer;
}

/**
 * @version 0.1
 * @brief rf24_read_reg
 * @author minhnhut-n
 * @function: support to read multiple register data from nrf24.
 */
void rf24_read_reg_mul(RF24_Handle *rf, uint8_t reg, uint8_t* buffer, uint8_t size)
{
    spi_beginTransaction(rf);
    uint8_t cmd = R_REG | (reg & 0x1F); //for ensuring reg not over 5 bits
    uint8_t dummy = NOP;
    uint8_t status;

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status, 1, RF_SPI_TIMEOUT) != HAL_OK) {
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

    if (status & RF24_TX_DF) {
        printf("[ERR] TX max retries!\r\n");
        return RF24_EVT_MAX_RT;
    }

    if (status & RF24_TX_DS) {
        printf("[INFO] TX is success!\r\n");
        return RF24_EVT_TX_OK;
    }

    if (status & RF24_RX_DR) {
        printf("[INFO] Having data in RX!\r\n");
        return RF24_EVT_RX_DR;
    }

    if (timed_out) {
        // timeout diagnostics + recovery (moved from rf24_transmit)
        printf("[ERR] Transmit timeout\r\n");
        uint8_t obs = rf24_read_reg(rf, OBSERVE_TX);
        printf("[DBG] OBSERVE_TX=0x%02X\r\n", obs);
        uint8_t cfg = rf24_read_reg(rf, CONFIG_REG);
        printf("[DBG] CONFIG=0x%02X (PRIM_RX=%d, PWR_UP=%d)\r\n", cfg, (cfg>>PRIM_RX)&1, (cfg>>PWR_UP)&1);
        uint8_t fifo = rf24_read_reg(rf, FIFO_STATUS);
        printf("[DBG] FIFO_STATUS=0x%02X\r\n", fifo);
        uint8_t txaddr[5] = {0};
        rf24_read_reg_mul(rf, TX_ADDR, txaddr, MAX_ADDRESS);
        printf("[DBG] TX_ADDR = "); for (int i=0;i<MAX_ADDRESS;i++) printf("%02X", txaddr[i]); printf("\r\n");
        uint8_t rx0[5] = {0};
        rf24_read_reg_mul(rf, RX_PIPE_ADDR_0, rx0, MAX_ADDRESS);
        printf("[DBG] RX_ADDR_P0 = "); for (int i=0;i<MAX_ADDRESS;i++) printf("%02X", rx0[i]); printf("\r\n");
        printf("[DBG] cmd_send_data=0x%02X, is_auto_ack=%d, is_pipe0_rx=%d, ce_status=%d\r\n",
               rf->cmd_send_data, rf->is_auto_ack, rf->is_pipe0_rx, rf->cfg.ce_status);

        rf24_clear_all_irq(rf);
        rf24_flush_tx_buffer(rf);
        rf24_flush_rx_buffer(rf);

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
 * @brief rf24_read_data
 * @author minhnhut-n
 * @function: used to read buffer from nrf24 with multiple bytes.
 */
void rf24_read_data(RF24_Handle *rf, uint8_t* buffer, uint8_t size)
{
    //dynamic payload check
    if (rf->dynamic_payload_enabled) {
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
    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &cmd, &status_reg, 1, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &dummy, buffer, size, RF_SPI_TIMEOUT) != HAL_OK) {
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
    uint8_t status = rf24_read_reg(rf, STATUS_REG);
    uint8_t rx_p_no = (status >> RX_P_NO) & 0x07;

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
    uint8_t rtn = rf24_read_reg(rf, SETUP_AW);

    rtn += 2;
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
    uint8_t config = rf24_read_reg(rf, RF_SETUP);

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
    uint8_t config = rf24_read_reg(rf, RF_SETUP);

    switch (baudrate)
    {
    case RF24_1MBPS:
        config &= ~(1<<RF_DR_HIGH);
        config &= ~(1<<RF_DR_LOW);
        break;
    case RF24_2MBPS:
        config |= (1<<RF_DR_HIGH);
        config &= ~(1<<RF_DR_LOW);
        break;
    case RF24_250KBPS:
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
    if (status) {
        rf->cfg.rf24_config_reg |= (1 << PWR_UP);
        rf->power_state = true;
    }
    else {
        rf->cfg.rf24_config_reg &= ~(1 << PWR_UP);
        rf->power_state = false;
    }

    rf24_write_reg(rf, CONFIG_REG, rf->cfg.rf24_config_reg);

    if (status) {
        HAL_Delay(5); //ref from community
    }
}

/**
 * @version 0.1
 * @brief rf24_power_amp_set
 * @author minhnhut-n
 * @function: set the strength of power amplifier from -18dBm to 0dBm
 */
void rf24_power_amp_set(RF24_Handle *rf, uint8_t level) {
    rf24_ce_pin(rf, false);
    uint8_t rf_setup = rf24_read_reg(rf, RF_SETUP);
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

    uint8_t config = rf24_read_reg(rf, CONFIG_REG);
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
 * @brief rf24_autoAck_enable
 * @author minhnhut-n
 * @function: auto-ack for 2 ways verifing and sending data (high reliable) 
 */
void rf24_autoAck_enable(RF24_Handle *rf, bool type)
{
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
    uint8_t feature = rf24_read_reg(rf, FEATURE);

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
    if (type) {
        rf->dynamic_payload_enabled = true;
        rf24_write_reg(rf, DYNPD, 0x3F); // enable dynamic payload trên tất cả pipe
    } else {
        rf->dynamic_payload_enabled = false;
        rf24_write_reg(rf, DYNPD, 0x00); // disable dynamic payload
    }
}

/**
 * @version 0
 * @brief rf24_addr_width_set
 * @author minhnhut-n
 * @function: wide of address frame from 3 to 5 bytes
 */
void rf24_addr_width_set(RF24_Handle *rf, uint8_t size)
{
    if (size > MAX_ADDRESS) {
        printf("AddrLen is too long(%d), default(5)\r\n", size);
        size = MAX_ADDRESS;
    }
    else if (size < 3) {
        printf("AddrLen is too short(%d), assigned(3)\r\n", size);
        size = 3;
    }
    rf->addr_len = size;
    size -= 2;
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

    uint8_t feature = rf24_read_reg(rf, FEATURE);
    if (feature == 0x00) {
        spi_beginTransaction(rf);
        uint8_t cmd[2] = {0x50, 0x73}; // ACTIVATE sequence
        uint8_t status;
        HAL_SPI_TransmitReceive(rf->cfg.hspi, cmd, &status, 2, RF_SPI_TIMEOUT);
        spi_endTransaction(rf);
        feature = rf24_read_reg(rf, FEATURE);
    }

    feature = rf24_read_reg(rf, FEATURE);
    if (write_with_ack) {
        rf->cmd_send_data = W_TX_PAYLOAD;
        feature &= ~(1 << EN_DYN_ACK);
    }
    else {
        rf->cmd_send_data = W_TX_PAYLOAD_NO_ACK;
        feature |= (1 << EN_DYN_ACK);
    }
    rf24_write_reg(rf, FEATURE, feature);
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
    rf->cfg.rf24_config_reg = rf24_read_reg(rf, CONFIG_REG);

    if ( !(rf->cfg.rf24_config_reg & (1 << PWR_UP)) ) {
        rf->cfg.rf24_config_reg |= (1 << PWR_UP);
        rf24_write_reg(rf, CONFIG_REG, rf->cfg.rf24_config_reg);
    }
    HAL_Delay(1);
}
/**
 * @version 0
 * @brief rf24_flush_tx_buffer
 * @author minhnhut-n
 * @function: flush TX buffer
 */
void rf24_flush_tx_buffer(RF24_Handle *rf)
{
    spi_beginTransaction(rf);
    uint8_t command = FLUSH_TX;
    uint8_t status;
    HAL_SPI_TransmitReceive(rf->cfg.hspi, &command, &status, 1, RF_SPI_TIMEOUT);
    spi_endTransaction(rf);
}
/**
 * @version 0
 * @brief rf24_flush_rx_buffer
 * @author minhnhut-n
 * @function: flush RX buffer
 */
void rf24_flush_rx_buffer(RF24_Handle *rf)
{  
    spi_beginTransaction(rf);
    uint8_t command = FLUSH_RX;
    uint8_t status;
    HAL_SPI_TransmitReceive(rf->cfg.hspi, &command, &status, 1, RF_SPI_TIMEOUT);
    spi_endTransaction(rf);
}






/**
 * @version 0
 * @brief rf24_carrier_wave_enable
 * @author minhnhut-n
 * @function: enable/disable continuous carrier wave output
 */
void rf24_carrier_wave_enable(RF24_Handle *rf, bool enable) {
    rf24_ce_pin(rf, false);
    uint8_t rf_setup = rf24_read_reg(rf, RF_SETUP);

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
    uint8_t rf_setup = rf24_read_reg(rf, RF_SETUP);

    if (enable) {
        rf_setup |= (1 << PLL_LOCK);
    } else {
        rf_setup &= ~(1 << PLL_LOCK);
    }
    rf24_write_reg(rf, RF_SETUP, rf_setup);
}
/**
 * @brief: check chip connect base on address have assigned.
 */
bool rf24_isChipConnected(RF24_Handle *rf) {
    uint8_t addr_width = rf24_read_reg(rf, SETUP_AW);
    return (addr_width == (rf->addr_len - 2) ? 1 : 0);
}


/**
 * @version 0.1
 * @brief rf24_transmit
 * @author minhnhut-n
 * @function: user-api to send the data
 */
uint8_t rf24_transmit(RF24_Handle *rf, const uint8_t* buffer, uint8_t size, bool ack_pay_required)
{
    uint8_t write_ok = rf24_write_data(rf, buffer, size, ack_pay_required);
    if (write_ok) printf("Success on write data!\r\n");

    uint8_t status_reg = rf24_read_reg(rf, STATUS_REG);
    uint32_t start = HAL_GetTick(); //ms

    // TX_DS : FIFO TX interrupt (package sent/ ack received(if ack enabled))
    // TX_DF : reach MAX retransmit times, need to reset
    // Need to finished soon (< 100us) estimate
    while (status_reg != (RF24_TX_DS | RF24_TX_DF)) {
        //not response
        if (HAL_GetTick() - start > 100) {
            printf("Asserted as fail!\r\n");
            return 0;
        }
    }

    rf24_ce_pin(rf, false);
    rf24_clear_all_irq(rf);

    if (status_reg & RF24_TX_DF) {
        printf("Max reties \r\n");
        rf24_flush_tx_buffer(rf);
        return 0;
    }

    return 1;
}

/**
 * @version 0.1
 * @brief rf24_write_data
 * @author minhnhut-n
 * @function: child process to write data into register
 */
uint8_t rf24_write_data(RF24_Handle *rf, const uint8_t* buffer, uint8_t size, bool ack_pay_required)
{
    if (ack_pay_required)
        rf->cmd_send_data = W_TX_PAYLOAD;
    else
        rf->cmd_send_data = W_TX_PAYLOAD_NO_ACK;

    //pre-processsing data
	uint8_t payloadTX[ONE_SECTION_BUF] = {0};
    if (rf->dynamic_payload_enabled) {
        size = MINVALUE(size, ONE_SECTION_BUF);
    	memcpy(payloadTX, buffer, size);
    } else {
        // padding by 0
        memset(payloadTX, 0, ONE_SECTION_BUF);
    	memcpy(payloadTX, buffer, size);
        size = ONE_SECTION_BUF;
    }

    uint8_t status = 0;
    spi_beginTransaction(rf);
    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &rf->cmd_send_data, &status, 1, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("[ERR] write fail: %02X \r\n", rf->cmd_send_data);
        spi_endTransaction(rf);
        return 0;
    }

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, payloadTX, (uint8_t*)NOP, size, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("[ERR] data sending\r\n");
        spi_endTransaction(rf);
        return 0;
    }
    spi_endTransaction(rf);
    return 1;
}
/**
 * @brief: check whether data is ready to read
 */
bool rf24_is_data_available(RF24_Handle *rf) {
    return (rf24_read_reg(rf, FIFO_STATUS) & 0x01) == 0;
}
/**
 * @brief: only 1 address that RF24 can send and auto ack at a time
 */
void rf24_tx_to_addr(RF24_Handle *rf, uint8_t *value, uint8_t size) {
    if (size < 3 || size > 5) return;
    // address setting for tx
    memcpy(rf->pipe0_tx_addr, value, size);
    rf->addr_len = size;
    rf24_write_reg_mul(rf, RX_PIPE_ADDR_0, rf->pipe0_tx_addr, rf->addr_len);
    rf24_write_reg_mul(rf, TX_ADDR, rf->pipe0_rx_addr, rf->addr_len);
}
/**
 * @version 0
 * @brief rf24_tx_mode
 * @author minhnhut-n
 * @function: used in switching mode from RX to TX
 */
void rf24_tx_mode(RF24_Handle *rf, const uint8_t* tx_address)
{
    rf->is_pipe0_rx = false;
    rf24_ce_pin(rf, false);
    delay_us(200);

    if (rf->is_ack_payload_enabled)
        rf24_flush_tx_buffer(rf);

    // switch to tx mode
    rf24_write_reg(rf, CONFIG_REG, (rf->cfg.rf24_config_reg & ~(1<<PRIM_RX)));

    // address setting for tx
    rf24_write_reg_mul(rf, RX_PIPE_ADDR_0, rf->pipe0_tx_addr, rf->addr_len);
    // enable rx pipe 0
    uint8_t enaa_reg = rf24_read_reg(rf, EN_AA) | (1 << 0);
    rf24_write_reg(rf, EN_AA, enaa_reg);    

    HAL_Delay(2);
}
/**
 * @version 0.1
 * @brief rf24_pipeData_rx_open
 * @author minhnhut-n
 * @function: for receiver application, open specific pipe to receive data
 */
void rf24_rx_pipe_open(RF24_Handle *rf, uint8_t pipeNum, uint64_t address)
{
    // store into cache, only enable when RX mode for pipe 0
    if (pipeNum == 0) {
        memcpy(rf->pipe0_rx_addr, &address, rf->addr_len);
        rf->is_restore_pipe0_addr = true;
    }

    //Due to PIPE 2->5, share 4 bytes address with PIPE1 (same)
    //It must be edit 1 LSB for address.
    //PIPE0 share address with TX_PIPE,
    //It must be not overwrite on TX_PIPE_ADDR when it is in TX mode
    uint8_t targetPipeAddr = pipeAddr[pipeNum];
    uint8_t value[MAX_ADDRESS] = {0};
    uint8_t config_reg = rf24_read_reg(rf, CONFIG_REG);
    rf->cfg.rf24_config_reg = config_reg;

    if (pipeNum <= 5) {
        if (pipeNum > 1) {
            // only 1 byte address available for upper pipe 1
        	value[0] = (uint8_t) (address & 0xFF);
            rf24_write_reg_mul(rf, targetPipeAddr, value, 1);
        }
        else if (rf->cfg.rf24_config_reg & (1 << PRIM_RX) || pipeNum != 0) {
            //pipe 0 can be use in autoack in TX mode, override pipe 0 addr -> can fail ack process
            memcpy(value, &address, rf->addr_len);
            rf24_write_reg_mul(rf, targetPipeAddr, value, rf->addr_len);
        }

        uint8_t enaa_reg = rf24_read_reg(rf, EN_AA) | (1 << pipeNum);
        rf24_write_reg(rf, EN_AA, enaa_reg);
    }
}
/**
 * @brief: function design to close specific pipe on RX mode
 */
static void rf24_closeRxPipe(RF24_Handle *rf, uint8_t pipe_num) {
    uint8_t pipe_en = rf24_read_reg(rf, EN_RXADDR);
    rf24_write_reg(rf, EN_RXADDR, pipe_en & ~(1 << pipe_num));

    if (pipe_num == 0) {
        rf->is_pipe0_rx = false;
    }
}
/**
 * @version 0
 * @brief rf24_rx_mode
 * @author minhnhut-n
 * @function
 */
void rf24_rx_mode(RF24_Handle *rf, uint8_t pipeNum, uint8_t* addressRX)
{
    rf->cfg.rf24_config_reg |= (1 << PRIM_RX);
    rf24_write_reg(rf, CONFIG_REG, rf->cfg.rf24_config_reg);

    rf24_clear_all_irq(rf);
    rf24_ce_pin(rf, true);

    // https://github.com/nRF24/RF24/issues/671
    // if previously pipe 0 is used to received package, it should be restore as user intentionally used.
    // otherwise we should avoid to use pipe 0 for listening packets. in case of we have a chain of rf24 communicate
    // the information is sent through among rf24 modules.
    if (rf->is_pipe0_rx) {
        rf24_write_reg_mul(rf, RX_PIPE_ADDR_0, rf->pipe0_rx_addr, rf->addr_len);
    }
    else {
        rf24_closeRxPipe(rf, 0);
    }
}
/**
 * This register is named ACTIVATE which is relate with FEATURE reg
 * but for some reason this was hidden.
 */
void rf24_toggle_feature(RF24_Handle *rf) {
    spi_beginTransaction(rf);
    //activate
    rf24_write_reg(rf, 0x50, 0x73);
    spi_endTransaction(rf);
}
/**
 * @version 0.1
 * @brief rf24_init
 * @author minhnhut-n
 * @function: all we know, that is init
 */
void rf24_init(RF24_Handle *rf)
{
    rf24_init_pins(rf);
    rf24_init_radio(rf);
}
/**
 * @version 0.1
 * @brief rf24_init
 * @author minhnhut-n
 * @function: all we know, that is init
 */
void rf24_init_radio(RF24_Handle *rf)
{
    printf("\n==== INIT RF24 RADIO ====\r\n");

    // WARNING: Delay is based on P-variant whereby non-P *may* require different timing.
    // Refer from RF24.h (communities versions)
    HAL_Delay(5);

    rf24_autoAck_config(rf, 4000, 5);
    rf24_baudrate_set(rf, RF24_1MBPS);

    uint8_t before = rf24_read_reg(rf, FEATURE);
    rf24_toggle_feature(rf);
    uint8_t after = rf24_read_reg(rf, FEATURE);
    rf->is_p_variant = before == after;

    //from communities
    if (after) {
        if (rf->is_p_variant) {
            // module did not experience power-on-reset (#401)
            rf24_toggle_feature(rf);
        }
        // allow use of multicast parameter and dynamic payloads by default
        rf24_write_reg(rf, FEATURE, 0);
    }

    rf->is_auto_ack = false;
    rf24_dynamic_payLoad(rf, false);
    rf24_autoAck_enable(rf, true);
    rf->payload_size = 32;
    rf24_addr_width_set(rf, MAX_ADDRESS);

    //channel by default is 76
    rf24_channel_set(rf, 76);

    rf24_clear_all_irq(rf);
    rf24_flush_rx_buffer(rf);
    rf24_flush_tx_buffer(rf);

    // Clear CONFIG register: (from community)
    //      Reflect all IRQ events on IRQ pin
    //      Enable PTX
    //      Power Up
    //      16-bit CRC (CRC required by auto-ack)
    // Do not write CE high so radio will remain in standby I mode
    // PTX should use only 22uA of power
    uint8_t info = 0x00;
    info = (1 << EN_CRC) | (1 << CRCO);
    rf24_write_reg(rf, CONFIG_REG, info);
    rf->cfg.rf24_config_reg = rf24_read_reg(rf, CONFIG_REG);

    //last thing power it on
    rf24_power_enable_set(rf, true);

    printf("==== END INIT RF24 ====\r\n");
}
/**
 * @version 0.1
 * @brief rf24_init
 * @author minhnhut-n
 * @function: all we know, that is init
 */
void rf24_init_pins(RF24_Handle *rf)
{
    printf("==== INIT RF24 PINS ====\r\n");

    rf24_ce_pin(rf, true);
    spi_endTransaction(rf);
    HAL_Delay(50);

    printf("==== END INIT RF24 ====\r\n");
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

    //flush fifo buffer
    printf("rf24_reset: flush buffer\r\n");
    rf24_flush_rx_buffer(rf);
    rf24_flush_tx_buffer(rf);
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
    uint8_t check = rf24_read_reg(rf, CONFIG_REG);
    printf("Value: %02X\r\n", check);

    printf("==>> AutoACK mode <<==\r\n");
    check = rf24_read_reg(rf, EN_AA);
    printf("Value: %02X\r\n", check);

    printf("==>> Address config <<==\r\n");
    check = rf24_read_reg(rf, SETUP_AW);
    printf("Value: %02X\r\n", check);

    printf("==>> Channel config <<==\r\n");
    check = rf24_read_reg(rf, RF_CH);
    printf("Value: %02X\r\n", check);

    printf("==>> Baudrate config <<==\r\n");
    check = rf24_read_reg(rf, RF_SETUP);
    printf("Value: %02X\r\n", check);

    printf("==>> Pipe Address	<<==\r\n");
    uint8_t buff[MAX_ADDRESS];
    uint8_t pipeChose = pipeAddr[pipeNum];
    rf24_read_reg_mul(rf, pipeChose, buff, MAX_ADDRESS);
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
    uint8_t check = rf24_read_reg(rf, CONFIG_REG);
    printf("Value: %02X\r\n", check);

    printf("==>> AutoACK mode <<==\r\n");
    check = rf24_read_reg(rf, EN_AA);
    printf("Value: %02X\r\n", check);

    printf("==>> Address config <<==\r\n");
    check = rf24_read_reg(rf, SETUP_AW);
    printf("Value: %02X\r\n", check);

    printf("==>> Channel config <<==\r\n");
    check = rf24_read_reg(rf, RF_CH);
    printf("Value: %02X\r\n", check);

    printf("==>> Baudrate config <<==\r\n");
    check = rf24_read_reg(rf, RF_SETUP);
    printf("Value: %02X\r\n", check);

    printf("==>> Pipe Address	<<==\r\n");
    uint8_t buff[MAX_ADDRESS];
    uint8_t pipeChose = pipeAddr[pipeNum];
    rf24_read_reg_mul(rf, pipeChose, buff, MAX_ADDRESS);
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

    v = rf24_read_reg(rf, CONFIG_REG);
    printf("CONFIG        : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, EN_AA);
    printf("EN_AA         : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, EN_RXADDR);
    printf("EN_RX_ADDR    : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, SETUP_AW);
    printf("SETUP_AW      : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, SETUP_RETR);
    printf("SETUP_RETR    : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, RF_CH);
    printf("RF_CH         : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, RF_SETUP);
    printf("RF_SETUP      : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, STATUS_REG);
    printf("STATUS        : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, OBSERVE_TX);
    printf("OBSERVE_TX    : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, RPD);
    printf("RPD           : 0x%02X\r\n", v);

    rf24_read_reg_mul(rf, RX_PIPE_ADDR_0, buf, 5);
    printf("RX_ADDR_P0    : ");
    for (int i = 0; i < 5; i++) printf("%02X ", buf[i]);
    printf("\r\n");

    rf24_read_reg_mul(rf, RX_PIPE_ADDR_1, buf, 5);
    printf("RX_ADDR_P1    : ");
    for (int i = 0; i < 5; i++) printf("%02X ", buf[i]);
    printf("\r\n");

    v = rf24_read_reg(rf, RX_PIPE_ADDR_2);
    printf("RX_ADDR_P2    : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, RX_PIPE_ADDR_3);
    printf("RX_ADDR_P3    : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, RX_PIPE_ADDR_4);
    printf("RX_ADDR_P4    : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, RX_PIPE_ADDR_5);
    printf("RX_ADDR_P5    : 0x%02X\r\n", v);

    rf24_read_reg_mul(rf, TX_ADDR, buf, 5);
    printf("TX_ADDR       : ");
    for (int i = 0; i < 5; i++) printf("%02X ", buf[i]);
    printf("\r\n");

    v = rf24_read_reg(rf, RX_PW_P0);
    printf("RX_PW_P0      : %d\r\n", v);

    v = rf24_read_reg(rf, RX_PW_P1);
    printf("RX_PW_P1      : %d\r\n", v);

    v = rf24_read_reg(rf, RX_PW_P2);
    printf("RX_PW_P2      : %d\r\n", v);

    v = rf24_read_reg(rf, RX_PW_P3);
    printf("RX_PW_P3      : %d\r\n", v);

    v = rf24_read_reg(rf, RX_PW_P4);
    printf("RX_PW_P4      : %d\r\n", v);

    v = rf24_read_reg(rf, RX_PW_P5);
    printf("RX_PW_P5      : %d\r\n", v);

    v = rf24_read_reg(rf, FIFO_STATUS);
    printf("FIFO_STATUS   : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, DYNPD);
    printf("DYNPD         : 0x%02X\r\n", v);

    v = rf24_read_reg(rf, FEATURE);
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
