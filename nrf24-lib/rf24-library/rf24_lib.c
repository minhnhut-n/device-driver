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
#define isEmptyBuffer(buf)      ((buf) != NULL ? 1 : 0)
#define minValue(val1, val2)	((val1) < (val2) ? (val1) : (val2))

static const uint8_t pipeAddr[6] = {RX_PIPE_ADDR_0, RX_PIPE_ADDR_1, RX_PIPE_ADDR_2,
                                    RX_PIPE_ADDR_3, RX_PIPE_ADDR_4, RX_PIPE_ADDR_5};
/**
 * Static function
 * This function will be use only on internal of this file,
 * No export API outside
 * 
 * =============================================================================
 */
static inline void rf24_clear_irq(RF24_Handle *rf)
{
    uint8_t clr = (V_MAX_RT | V_RX_DR | V_TX_DS);
    rf24_write_reg(rf, STATUS_REG, clr);
}

static inline uint32_t convert_us_to_tick(uint32_t us) {
    return us*(SystemCoreClock/1e6);
}

static inline uint32_t convert_tick_to_us(uint32_t tick) {
    return tick/(SystemCoreClock/1e6);
}

static void software_reset(void)
{
    __disable_irq();        // optional nhưng nên có
    NVIC_SystemReset();     // reset toàn bộ hệ thống
}
/**
 * =============================================================================
 * MAIN FUNCTION
 * =============================================================================
 */

/**
 * @brief helper function for stm32-spi purpose
 * in this function, csn (chip select pin will go low to enable transmittion)
 */
void spi_beginTransaction(RF24_Handle *rf)
{
    HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, GPIO_PIN_RESET);
}

/**
 * @brief helper function for stm32stm32-spi purpose
 * in this function, csn (chip select pin will go high to disable transmittion)
 */
void spi_endTransaction(RF24_Handle *rf)
{
    HAL_GPIO_WritePin(rf->cfg.csnPort, rf->cfg.csnPin, GPIO_PIN_SET);
}

/**
 * @brief function support for writing configuration wih spi
 */
void rf24_write_reg(RF24_Handle *rf, uint8_t reg, uint8_t regData)
{
    rf24_ce_pin(rf, DISABLE);
    spi_beginTransaction(rf);
    uint8_t cmd = W_REG | (reg & 0x1F); //for ensuring reg not over 5 bits

    if (HAL_SPI_Transmit(rf->cfg.hspi, &cmd, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write config %02X \r\n", reg);
    }

    if (HAL_SPI_Transmit(rf->cfg.hspi, &regData, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write config data %02X \r\n", reg);
    }
    spi_endTransaction(rf);
}

/**
 * @brief function support for writing configuration wih spi
 */
void rf24_write_reg_mul(RF24_Handle *rf, uint8_t reg, const uint8_t* regData, uint8_t size)
{
    rf24_ce_pin(rf, DISABLE);
    spi_beginTransaction(rf);
    uint8_t cmd = W_REG | (reg & 0x1F); //for ensuring reg not over 5 bits

    if (HAL_SPI_Transmit(rf->cfg.hspi, &cmd, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write config %02X \r\n", reg);
    }

    if (HAL_SPI_Transmit(rf->cfg.hspi, regData, size, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write config data %02X \r\n", reg);
    }
    spi_endTransaction(rf);
}

/**
 * @brief function support for reading configuration wih spi
 */
void rf24_read_reg(RF24_Handle *rf, uint8_t reg, uint8_t* buffer, uint8_t size)
{
    spi_beginTransaction(rf);
    uint8_t cmd = R_REG | (reg & 0x1F); //for ensuring reg not over 5 bits
    uint8_t dummy = NOP;

    if (HAL_SPI_Transmit(rf->cfg.hspi, &cmd, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when read config %02X \r\n", reg);
    }

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &dummy, buffer, size, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when read config data %02X \r\n", reg);
    }
    spi_endTransaction(rf);   
}

uint8_t rf24_transmit(RF24_Handle *rf, const uint8_t* buffer, uint8_t size)
{
    bool result = false;
    rf24_tx_mode(rf, rf->tx_addr);

    // static uint8_t count = 0;
    // count ++;
    // printf("\nsend %dth times", count);
    // rf24_dump_registers(rf);

    //sending
    rf24_write_data(rf, buffer, size);

    //observe after sending
    uint8_t observe;  //check retries count
    uint8_t status; //check TX_DS,... flag
    uint32_t start = HAL_GetTick();
    const uint32_t timeout = 1000;

    do {
        rf24_read_reg(rf, OBSERVE_TX, &observe, ONE_BYTE);
        rf24_read_reg(rf, STATUS_REG, &status, ONE_BYTE);
        printf("Observe_TX: %02X\r\n", observe);
    } while ( !(status & (V_TX_DS|MAX_RT)) && (HAL_GetTick() - start < timeout) );

    if (status & V_TX_DS) {
        printf("[INFO] TX is oke!\r\n");
        result = 1;
    }
    if (status & V_MAX_RT) {
        printf("[ERR] TX max retries!\r\n");
        result = 0;
    }
    if (status & V_RX_DR) {
        printf("[INFO] Having data in RX!\r\n");
    }

    //POWER DOWN
    rf24_power_enable_set(rf, false);
    //Flush TX buffer
    rf24_empty_tx_buffer(rf);
    //Flush RX buffer
    rf24_empty_rx_buffer(rf);
    return result;
}

/**
 * @brief RF24 have payload buffer, check this setting for payload and override data on this
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
        size = minValue(size, ONE_SECTION_BUF);
    	memcpy(payloadTX, buffer, size);
    } else {
        memset(payloadTX, 0, ONE_SECTION_BUF);
    	memcpy(payloadTX, buffer, size);
        size = ONE_SECTION_BUF;
    }

    //condition on write data
    rf24_ce_pin(rf, DISABLE);
    //spi transmission set
    spi_beginTransaction(rf);
    uint8_t cmd = W_PAY_LOAD;

    if (HAL_SPI_Transmit(rf->cfg.hspi, &cmd, ONE_BYTE, SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
        return 0;
    }

    if (HAL_SPI_Transmit(rf->cfg.hspi, payloadTX, size, SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
        return 0;
    }
    spi_endTransaction(rf);
    // ========== STEP 4: Pulse CE to transmit ==========
    rf24_ce_pin(rf, ENABLE);
    delay_us(20);
    rf24_ce_pin(rf, DISABLE);

    return 1;
}

/**
 * @brief function support for reading user data wih spi
 */
void rf24_read_data(RF24_Handle *rf, uint8_t* buffer, uint8_t size)
{
    //dynamic payload check
    if (rf->dynamic_pay_load) {
        size = minValue(size, ONE_SECTION_BUF);
    }
    else {
        size = ONE_SECTION_BUF;
    }

    rf24_ce_pin(rf, DISABLE);    
    //receive action
    spi_beginTransaction(rf);
    uint8_t cmd = R_PAY_LOAD;
    uint8_t dummy = NOP;
    if (HAL_SPI_Transmit(rf->cfg.hspi, &cmd, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &dummy, buffer, size, SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }
    spi_endTransaction(rf);
}

/**
 * @brief Check data available or not
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
 * @brief Using SETUP_AW to check it is value or not.
 * Value on this REG can be 1,2,3 respectively with 3,4,5 bytes
 * So the offset is = -2
 */
bool isValid_AddrWidth(RF24_Handle *rf)
{
    rf24_ce_pin(rf, DISABLE);    
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
 * @brief Power consumption for rf24
 */
void rf24_PA_set(RF24_Handle *rf, uint8_t level)
{
    rf24_ce_pin(rf, DISABLE);
    uint8_t config = 0;
    rf24_read_reg(rf, RF_SETUP, &config, ONE_BYTE);

    config &= ~((1<<1) | (1<<2));
    config |= ((level&0x03) << 1) | 0x01;

    rf24_write_reg(rf, RF_SETUP, config);
}

/**
 * @brief Channel set for rf24
 */
void rf24_channel_set(RF24_Handle *rf, uint8_t channel)
{
    rf24_ce_pin(rf, DISABLE);
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
 * @brief Baudrate set for rf24
 */
void rf24_baudrate_set(RF24_Handle *rf, uint8_t baudrate)
{
    rf24_ce_pin(rf, DISABLE);
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
 * @brief Power set for rf24
 */
void rf24_power_enable_set(RF24_Handle *rf, uint8_t status)
{
    uint8_t config = 0;
    rf24_ce_pin(rf, DISABLE);
    rf24_read_reg(rf, CONFIG_REG, &config, ONE_BYTE);

    if (status) {
        config |= (1 << PWR_UP);
    }
    else {
        config &= ~(1 << PWR_UP);
    }

    rf24_write_reg(rf, CONFIG_REG, config);
    
    if (status) {
        HAL_Delay(2);  // Wait for power-up (1.5ms minimum)
    }
}

/**
 * @brief: power amplifier
 */
void rf24_power_amp_set(RF24_Handle *rf, uint8_t level) {
    uint8_t rf_setup;
    rf24_ce_pin(rf, DISABLE);
    
    rf24_read_reg(rf, CONFIG_REG, &rf_setup, ONE_BYTE);
    //reset
    rf_setup &= ~(3 << 1);
    //set
    rf_setup |= (level << 1);
    rf24_write_reg(rf, CONFIG_REG, rf_setup);
}

/**
 * @brief: CRC setting for payload transmit
 */
void rf24_crc_setting(RF24_Handle *rf, bool enable, uint8_t numCRCByte)
{
    rf24_ce_pin(rf, DISABLE);

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
 * @brief change pin CE logic and status
 * @def update CE pin logic and status
 */
void rf24_ce_pin(RF24_Handle *rf, bool status)
{
    if (status)
    {
        HAL_GPIO_WritePin(rf->cfg.cePort, rf->cfg.cePin, ENABLE);
        rf->cfg.ce_status = true;
    }
    else
    {
        HAL_GPIO_WritePin(rf->cfg.cePort, rf->cfg.cePin, DISABLE);
        rf->cfg.ce_status = false;
    }
}

/**
 * @brief open pipe data for reading
 * @def this logic is need to check 2 things:
 * - pipe 0 is reset address or not (read on datasheet, pipe 0 have 2 missionx)
 * - which pipe is used to reading?
 */
void rf24_pipeData_rx_open(RF24_Handle *rf, uint8_t pipeNum, const uint8_t* addressRX)
{
    rf24_ce_pin(rf, DISABLE);    
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

            //Addr
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
 * Close pipe data for reading
 */
void rf24_pipeData_rx_close(RF24_Handle *rf, uint8_t pipeNum)
{
    rf24_ce_pin(rf, DISABLE);    
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
 * @brief auto-ack setting
 */
void rf24_autoAck_enable(RF24_Handle *rf, bool type)
{
    rf24_ce_pin(rf, DISABLE);    
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
 * @brief Auto Acknowledgment configuration for rf24
 */
void rf24_autoAck_config(RF24_Handle *rf, uint16_t ack_time, uint8_t ack_retry)
{
    rf24_ce_pin(rf, DISABLE);    
    if (ack_time < 250) {
        printf("rate is not valid, set to default\r\n");
        ack_time = 250;
    }

    uint8_t config = 0x00;  
    config |= ((uint8_t)(ack_time / 250) - 1) << 4 | (ack_retry << 0);
    rf24_write_reg(rf, SETUP_RETR, config);
}

void rf24_ack_payload(RF24_Handle *rf, bool is_ack_payload) {
    rf24_ce_pin(rf, DISABLE);
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

void rf24_dynamic_payLoad(RF24_Handle *rf, bool type) {
    rf24_ce_pin(rf, DISABLE);

    uint8_t feature = 0x00;
    rf24_read_reg(rf, FEATURE, &feature, ONE_BYTE);
    if (type) {
        feature |= (1 << EN_DPL);
        rf->dynamic_pay_load = true;
    } else {
        feature &= ~(1 << EN_DPL);
        rf->dynamic_pay_load = false;
    }
    rf24_write_reg(rf, FEATURE, feature);
}

void rf24_addr_width_set(RF24_Handle *rf, uint8_t size) {
    rf24_ce_pin(rf, DISABLE);
    size &= 0x03;
    rf24_write_reg(rf, SETUP_AW, size);
}

void rf24_cmd_on_write(RF24_Handle *rf, bool write_with_ack) {
    rf24_ce_pin(rf, DISABLE);

    uint8_t feature = 0x00;
    rf24_read_reg(rf, FEATURE, &feature, ONE_BYTE);
    if (write_with_ack) {
        rf->cmd_send_data = W_PAY_LOAD;
        feature &= ~(1 << EN_DYN_ACK);
    }
    else {
        rf->cmd_send_data = W_NO_ACK_PAYLD;
        feature |= (1 << EN_DYN_ACK);
    }
    rf24_write_reg(rf, FEATURE, feature);
}

/**
 * @brief Configuration mode RX on RF24
 * @def change to RX mode -> TX is disable
 */
void rf24_rx_mode(RF24_Handle *rf, uint8_t pipeNum, uint8_t* addressRX)
{
    rf->is_tx_mode = false;
    rf24_ce_pin(rf, DISABLE);
        
    //reset status register
    rf24_reset(rf);

    //enable address pipe
    rf24_pipeData_rx_open(rf, pipeNum, addressRX);

    //config register to rx mode
    rf24_read_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    rf->cfg.rf24_config_reg |= (RX_MODE << PRIM_RX);
    if (  !(rf->cfg.rf24_config_reg & (1 << PWR_UP)) ) {
        rf->cfg.rf24_config_reg |= (1 << PWR_UP);
    }
    
    rf24_write_reg(rf, CONFIG_REG, rf->cfg.rf24_config_reg);

    rf24_ce_pin(rf, ENABLE);
}

static void rf24_tx_addr_setting(RF24_Handle *rf, const uint8_t* tx_address) {
    
    rf->is_tx_mode = true;
    rf24_ce_pin(rf, DISABLE);
    memcpy(rf->tx_addr, tx_address, MAX_ADDRESS);

    //write tx address
    rf24_write_reg_mul(rf, TX_ADDR, rf->tx_addr, MAX_ADDRESS);
    //write ack address (pipe0)
    rf24_write_reg_mul(rf, RX_PIPE_ADDR_0, rf->tx_addr, MAX_ADDRESS);
    
    if (rf->is_auto_ack) {
        rf24_autoAck_config(rf, RE_ACK_TIME, RE_ACK_COUNT);
    }
}

/**
 * @brief Configuration mode TX on RF24
 * @def reverse with rx mode
 */
void rf24_tx_mode(RF24_Handle *rf, const uint8_t* tx_address)
{
    rf24_tx_addr_setting(rf, tx_address);

    //power up and set to tx mode
    uint8_t config = 0;
    rf24_read_reg(rf, CONFIG_REG, &config, ONE_BYTE);
    bool was_powered_down = !(config & (1 << PWR_UP));
    if ( was_powered_down ) {
        config |= (1 << PWR_UP);
    }

    if ( config & (1 << PRIM_RX) ) {
        config &= ~(1 << PRIM_RX);
    }
    rf24_write_reg(rf, CONFIG_REG, config);

    if( was_powered_down ) {
        HAL_Delay(2);
    }

    rf24_empty_tx_buffer(rf);
    rf24_empty_rx_buffer(rf);
}

/**
 * @brief Configuration mode STANDBY on RF24
 * @def move it back to standby mode ( waiting to hook )
 */
void rf24_standby_mode(RF24_Handle *rf)
{
    rf24_ce_pin(rf, DISABLE);
    rf24_read_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    if ( !(rf->cfg.rf24_config_reg & (1 << PWR_UP)) ) {
        rf->cfg.rf24_config_reg |= (1 << PWR_UP);
        rf24_write_reg(rf, CONFIG_REG, rf->cfg.rf24_config_reg);
    }

    rf->is_tx_mode = false;
    HAL_Delay(1);
}

/**
 * @brief Empty buffer TX
 */
void rf24_empty_tx_buffer(RF24_Handle *rf)
{
    rf24_ce_pin(rf, DISABLE);    
    spi_beginTransaction(rf);
    uint8_t command = FLUSH_TX;
    HAL_SPI_Transmit(rf->cfg.hspi, &command, ONE_BYTE, SPI_TIMEOUT);
    spi_endTransaction(rf);
}

/**
 * @brief Empty buffer RX
 */
void rf24_empty_rx_buffer(RF24_Handle *rf)
{
    rf24_ce_pin(rf, DISABLE);    
    spi_beginTransaction(rf);
    uint8_t command = FLUSH_RX;
    HAL_SPI_Transmit(rf->cfg.hspi, &command, ONE_BYTE, SPI_TIMEOUT);
    spi_endTransaction(rf);
}

/**
 * @brief: init rf24 module
 * @note:
 * - disable ce pin
 * - config reg to 0x00
 * - no auto ack
 * - disable rx addr
 * - channel reset to 0
 * - flush buffer (clear buffer)
 * - data rate and power reset to default (2MBps, 0dBm)
 */
void rf24_init(RF24_Handle *rf)
{
    printf("\n====  INIT RF24   ====\r\n");

    rf24_ce_pin(rf, false);
    spi_endTransaction(rf);

    delay_us(20);

    //RESET ALL REG TO INITIAL
    rf24_reset(rf);

    rf24_autoAck_config(rf, 1500, 15);
    rf24_crc_setting(rf, true, RF24_CRC_16);
    //address default is 5 bytes, but set for fun
    rf24_addr_width_set(rf, ADDR_5_BYTE);
    rf24_baudrate_set(rf, BAUD_1MBPS);
    rf24_power_amp_set(rf, MAX_POWER);
    rf24_autoAck_enable(rf, false);
    rf24_dynamic_payLoad(rf, false);

    rf24_power_enable_set(rf, true);
    printf("Status CE pin: %d\r\n", rf->cfg.ce_status);

    printf("====  END INIT RF24   ====\r\n");
}

/**
 * @brief: reset rf24 module (old)
 * @note:
 */
// void rf24_reset(RF24_Handle *rf, uint8_t reg)
// {
//     if (reg == STATUS_REG) {
//     	uint8_t clr = (1<<TX_DS) | (1<<MAX_RT) | (1<<RX_DR);
//     	rf24_write_reg(rf, STATUS_REG, clr);
//     }
//     else if (reg == FIFO_STATUS) {
//         uint8_t reset_val = 0x11;
//         rf24_write_reg(rf, FIFO_STATUS, reset_val);
//     }
//     else {
//         printf("rf24_reset: others register reset\r\n");
//         rf24_write_reg(rf, CONFIG_REG,   0x08);
//         rf24_write_reg(rf, EN_AA,        0x3F);
//         rf24_write_reg(rf, EN_RXADDR,    0x03);
//         rf24_write_reg(rf, SETUP_AW,     0x03);
//         rf24_write_reg(rf, SETUP_RETR,	 0x03);
//         rf24_write_reg(rf, RF_CH,        0x02);
//         rf24_write_reg(rf, RF_SETUP,     0x0E);
//         uint8_t clr = (1<<TX_DS) | (1<<MAX_RT) | (1<<RX_DR);
//         rf24_write_reg(rf, STATUS_REG, clr);
//         rf24_write_reg(rf, OBSERVE_TX,    0x00);
//         rf24_write_reg(rf, RPD,           0x00);

//         uint8_t rx_addr_p0_def[5];
//         memcpy(rx_addr_p0_def, rf->pipe0_rx_addr, MAX_ADDRESS);
//         rf24_write_reg_mul(rf, RX_PIPE_ADDR_0, rx_addr_p0_def, MAX_ADDRESS);

//         uint8_t rx_addr_p1_def[5];
//         memcpy(rx_addr_p1_def, rf->pipe1_rx_addr, MAX_ADDRESS);
//         rf24_write_reg_mul(rf, RX_PIPE_ADDR_1, rx_addr_p1_def, MAX_ADDRESS);

//         rf24_write_reg(rf, RX_PIPE_ADDR_2, 0xC3);
//         rf24_write_reg(rf, RX_PIPE_ADDR_3, 0xC4);
//         rf24_write_reg(rf, RX_PIPE_ADDR_4, 0xC5);
//         rf24_write_reg(rf, RX_PIPE_ADDR_5, 0xC6);

//         uint8_t tx_addr_def[5];
//         memcpy(tx_addr_def, rf->tx_addr, MAX_ADDRESS);
//         rf24_write_reg_mul(rf, TX_ADDR, tx_addr_def, MAX_ADDRESS);

//         rf24_write_reg(rf, RX_PW_P0, 0x00);
//         rf24_write_reg(rf, RX_PW_P1, 0x00);
//         rf24_write_reg(rf, RX_PW_P2, 0x00);
//         rf24_write_reg(rf, RX_PW_P3, 0x00);
//         rf24_write_reg(rf, RX_PW_P4, 0x00);
//         rf24_write_reg(rf, RX_PW_P5, 0x00);

//         rf24_write_reg(rf, FIFO_STATUS, 0x11);
//         rf24_write_reg(rf, DYNPD,       0x00);
//         rf24_write_reg(rf, FEATURE,     0x00);
//     }
// }

/**
 * @brief: reset rf24 module (new method)
 * @note:
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

void print_tc_function(RF24_Handle *rf, uint8_t pipeNum) {
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
