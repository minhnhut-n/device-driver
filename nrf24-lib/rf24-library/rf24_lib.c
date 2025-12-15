/*
 * rf24_lib.c
 *
 *  Created on: Nov 20, 2025
 *      Author: Aelius_Nguyen
 */

#include "rf24_lib.h"

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
    uint8_t clr = (1<<TX_DS) | (1<<MAX_RT) | (1<<RX_DR);
    rf24_write_reg(rf, STATUS_REG, &clr, 1);
}

void rf24_autoAck_enable(RF24_Handle *rf, bool type)
{
    rf24_ce_pin(rf, BIT_DISABLE);
    uint8_t config = 0;

    if (type)
    {
        config = 0x3F; //enable all pipe
        rf->is_auto_ack = true;
    }
    else
    {
        config = 0x00; //disable all pipe
        rf->is_auto_ack = false;   
    }

    rf24_write_reg(rf, EN_AA, &config, ONE_BYTE);
    rf24_ce_pin(rf, BIT_ENABLE);
}

/**
 * @brief Auto Acknowledgment configuration for rf24
 */
void rf24_autoAck_config(RF24_Handle *rf)
{
    uint8_t config = 0;
    config |= (RE_ACK_TIME / 250) << 4 | (RE_ACK_COUNT << 0);
    rf24_write_reg(rf, SET_AUTO_RETRS, &config, ONE_BYTE);
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
void rf24_write_reg(RF24_Handle *rf, uint8_t reg, const uint8_t *regData, uint8_t size)
{
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

/**
 * @brief RF24 have payload buffer, check this setting for payload and override data on this
 */
uint8_t rf24_write_data(RF24_Handle *rf, const uint8_t* buffer, uint8_t size)
{
    if (buffer == NULL) {
        printf("rf24_write_data: buffer is NULL -> abort\r\n");
        return 0;
    }

    if (rf->dynamic_pay_load) {
        if (rf->payload_size == 0) {
            printf("rf24_write_data: payload_size == 0 (not initialized)\r\n");
            return 0;
        }
        size = minValue(size, rf->payload_size);
    } else {
        size = minValue(size, ONE_SECTION_BUF);
    }

    //clear bit IQR of TX before sending
    rf24_clear_irq(rf);

    //Transmission set
    spi_beginTransaction(rf);
    uint8_t cmd = W_PAY_LOAD;
    if (HAL_SPI_Transmit(rf->cfg.hspi, &cmd, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
        return 0;
    }

    if (HAL_SPI_Transmit(rf->cfg.hspi, buffer, size, SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
        return 0;
    }
    spi_endTransaction(rf);

    rf24_ce_pin(rf, BIT_ENABLE);
    HAL_Delay(1);
    rf24_ce_pin(rf, BIT_DISABLE);

    uint8_t status = 0;
    rf24_read_reg(rf, STATUS_REG, &status, ONE_BYTE);
   
    if (status & (1 << TX_DS))
    {
        printf("Flag interrupt is clear!\r\n");
        rf24_clear_irq(rf);
        return 1;
    }
    else
    {
        printf("TX Failed or Timeout\r\n");
    }
    //check fail
    if (status & (1 << MAX_RT))
    {
        printf("TX failed (MAX_RT)\r\n");
    }

    rf24_clear_irq(rf);
    rf24_empty_tx_buffer(rf);

    return 0;
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

    rf24_ce_pin(rf, BIT_DISABLE);
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
    rf24_ce_pin(rf, BIT_ENABLE);
}

/**
 * @brief Check data available or not
 */
bool rf24_is_dataAvailable(RF24_Handle *rf, uint8_t pipeNum)
{
    uint8_t status = 0;
    rf24_read_reg(rf, STATUS_REG, &status, ONE_BYTE);

    if ((status & (1 << RX_DR)) && (status & (1 << pipeNum))) {
        //reset rx_dr flag
        status |= (1 << RX_DR);
        rf24_write_reg(rf, STATUS_REG, &status, ONE_BYTE);
        
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
    uint8_t rtn = 0;
    rf24_read_reg(rf, ADDR_WID, &rtn, ONE_BYTE);

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
void rf24_powerConsumption_set(RF24_Handle *rf)
{
    uint8_t config = 0;
    rf24_read_reg(rf, RF_SETUP, &config, ONE_BYTE);
    printf("power consumtion RF_SETUP info: %02x\r\n", config);

    config &= ~((1<<1) | (1<<2));
    config |= ((rf->power_amplifier&0x03) << 1);

    rf24_write_reg(rf, RF_SETUP, &config, ONE_BYTE);
    printf("power consumtion RF_SETUP after info: %02x\r\n", config);
}

/**
 * @brief Channel set for rf24
 */
void rf24_channel_set(RF24_Handle *rf, uint8_t channel)
{
    uint8_t reset = 0;
    rf24_read_reg(rf, RF_CH, &reset, ONE_BYTE);

    if(channel > 125 || channel < 0)
    {
        printf("Channel set invalid\r\n");
        return;
    }
    rf24_write_reg(rf, RF_CH, &channel, ONE_BYTE);
}

/**
 * @brief Baudrate set for rf24
 */
void rf24_baudrate_set(RF24_Handle *rf, uint8_t baudrate)
{
    uint8_t config = 0;
    rf24_read_reg(rf, RF_SETUP, &config, ONE_BYTE);
    printf("Baudrate set RF_CH info: %02x\r\n", config);

    if ( (baudrate & 0x01) != 0 ) {
        config |= (1<<RF_DR_HIGH);
    }
    else {
        config &= ~(1<<RF_DR_HIGH);
    }

    if ( (baudrate & 0x02) != 0 ) {
        config |= (1<<RF_DR_LOW);
    }
    else {
        config &= ~(1<<RF_DR_LOW);
    }

    printf("Baudrate set RF_SETUP info: %02x\r\n", config);
    rf24_write_reg(rf, RF_SETUP, &config, ONE_BYTE);
}

/**
 * @brief Power set for rf24
 */
void rf24_PA_set(RF24_Handle *rf, uint8_t status)
{
    uint8_t config = 0;
    rf24_read_reg(rf, RF_SETUP, &config, ONE_BYTE);
    printf("Power set RF_SETUP info: %02x\r\n", config);

    if (status) {
        config |= (1 << PWR_UP);
    }
    else {
        config &= ~(1 << PWR_UP);
    }

    printf("Power set RF_SETUP after info: %02x\r\n", config);
    rf24_write_reg(rf, RF_SETUP, &config, ONE_BYTE);
}

/**
 * @brief change pin CE logic and status
 * @def update CE pin logic and status
 */
void rf24_ce_pin(RF24_Handle *rf, bool status)
{
    if (status)
    {
        HAL_GPIO_WritePin(rf->cfg.cePort, rf->cfg.cePin, BIT_ENABLE);
        rf->cfg.ce_status = true;
    }
    else
    {
        HAL_GPIO_WritePin(rf->cfg.cePort, rf->cfg.cePin, BIT_DISABLE);
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
            rf24_write_reg(rf, targetPipeAddr, &addr, ONE_BYTE);
        }
        else if (pipeNum == PIPE1 || !(rf->is_tx_mode)) {
            rf24_write_reg(rf, targetPipeAddr, addressRX, MAX_ADDRESS);
        }
    }

    // Set payload width for the pipe
    uint8_t pw_reg = RX_PW_P0 + pipeNum;
    if (rf->dynamic_pay_load) {
        value = rf->payload_size; // Dynamic payload size
    } else {
        value = ONE_SECTION_BUF; // Static payload size
    }
    printf("Set payload size for PIPE %d: %d bytes\r\n", pipeNum, value);
    rf24_write_reg(rf, pw_reg, &value, ONE_BYTE);

    rf24_read_reg(rf, EN_RX_ADDR, &value, ONE_BYTE);
    value |= (ENABLE << pipeNum);
    rf24_write_reg(rf, EN_RX_ADDR, &value, ONE_BYTE);
}

/**
 * Close pipe data for reading
 */
void rf24_pipeData_rx_close(RF24_Handle *rf, uint8_t pipeNum)
{
    uint8_t value = 0;
    rf24_read_reg(rf, EN_RX_ADDR, &value, ONE_BYTE);
    value &= ~(ENABLE << pipeNum);
    rf24_write_reg(rf, EN_RX_ADDR, &value, ONE_BYTE);

    if (pipeNum == 0) {
        // keep track of pipe 0's RX state to avoid null vs 0 in addr cache
        rf->is_restore_pipe0_addr = false;
    }
    printf("PIPE%d is close!\r\n", pipeNum);
}

/**
 * Registry for TX tunnel prepare for writing
 */
void rf24_pipeData_tx_registry(RF24_Handle *rf, const uint8_t* address)
{
    rf24_write_reg(rf, RX_PIPE_ADDR_0, address, MAX_ADDRESS);
    rf24_write_reg(rf, TX_ADDR, address, MAX_ADDRESS);
}

/**
 * @brief Configuration mode RX on RF24
 * @def change to RX mode -> TX is disable
 */
void rf24_rx_mode(RF24_Handle *rf, uint8_t pipeNum, uint8_t* addressRX)
{
    rf->is_tx_mode = false;
    printf("Switch to RX mode on PIPE %d\r\n", pipeNum);

    //disable before config
    rf24_ce_pin(rf, BIT_DISABLE);
    
    //reset status register
    rf24_reset(rf, STATUS_REG);
    
    //write channel on receive mode
    rf24_channel_set(rf, rf->channel);

    //enable address pipe
    rf24_pipeData_rx_open(rf, pipeNum, addressRX);

    //config register to rx mode
    rf24_read_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    rf->cfg.rf24_config_reg |= (RX_MODE << PRIM_RX);
    if (  !(rf->cfg.rf24_config_reg & (1 << PWR_UP)) ) {
        rf->cfg.rf24_config_reg |= (1 << PWR_UP);
    }
    
    rf24_write_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    rf24_ce_pin(rf, BIT_ENABLE);
}

/**
 * @brief Configuration mode TX on RF24
 * @def reverse with rx mode
 */
void rf24_tx_mode(RF24_Handle *rf)
{
    rf->is_tx_mode = true;

    //disable before config
    rf24_ce_pin(rf, BIT_DISABLE);
    //write channel on transmit mode
    rf24_channel_set(rf, rf->channel);
    //write tx address
    rf24_write_reg(rf, TX_ADDR, rf->tx_addr, MAX_ADDRESS);
    //write ack address (pipe0)
    rf24_write_reg(rf, RX_PIPE_ADDR_0, rf->tx_addr, MAX_ADDRESS);

    //power up and set to tx mode
    uint8_t config = 0;
    rf24_read_reg(rf, CONFIG_REG, &config, ONE_BYTE);
    if ( !(config & (1 << PWR_UP)) ) {
        config |= (1 << PWR_UP);
    }
    if ( config & (1 << PRIM_RX) ) {
        config &= ~(1 << PRIM_RX);
    }
    rf24_write_reg(rf, CONFIG_REG, &config, ONE_BYTE);

    //save config for later use
    rf->cfg.rf24_config_reg = config;
}

/**
 * @brief Configuration mode STANDBY on RF24
 * @def move it back to standby mode ( waiting to hook )
 */
void rf24_standby_mode(RF24_Handle *rf)
{
    rf24_ce_pin(rf, false);
    rf24_read_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);
    printf("config reg info: %02x\r\n", rf->cfg.rf24_config_reg);

    if ( !(rf->cfg.rf24_config_reg & (1 << PWR_UP)) ) {
        rf->cfg.rf24_config_reg |= (1 << PWR_UP);
        rf24_write_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);
        printf("config reg after info: %02x\r\n", rf->cfg.rf24_config_reg);
    }

    rf->is_tx_mode = false;
    HAL_Delay(1);
}

/**
 * @brief Empty buffer TX
 */
void rf24_empty_tx_buffer(RF24_Handle *rf)
{
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

    uint8_t reset_val = 0x00;

    //disable ce pin
    rf24_ce_pin(rf, BIT_DISABLE);
    
    //config later
    rf24_write_reg(rf, CONFIG_REG, &reset_val, ONE_BYTE);
    
    //disable rx addr
    rf24_write_reg(rf, EN_RX_ADDR, &reset_val, ONE_BYTE);
    
    //address width reset to user specific
    uint8_t aw_val = MAX_ADDRESS - ADDR_WD_OFFSET;
    rf24_write_reg(rf, ADDR_WID, &aw_val, ONE_BYTE);
    
    //channel reset to 0
    rf24_channel_set(rf, reset_val);
    
    //data rate and power reset to default (2MBps, 0dBm)
    uint8_t rf_setup_val = 0x0E; //0000 1110
    rf24_write_reg(rf, RF_SETUP, &rf_setup_val, ONE_BYTE);

    //flush buffer (clear buffer)
    rf24_empty_rx_buffer(rf);
    rf24_empty_tx_buffer(rf);

    rf24_autoAck_enable(rf, true);
    if (rf->is_auto_ack) {
        // set auto ack configuration
        rf24_autoAck_config(rf);
    }

    //enable ce pin again after init
    rf24_ce_pin(rf, BIT_ENABLE);

    printf("====  END INIT RF24   ====\r\n");
}

/**
 * @brief: reset rf24 module
 * @note:
 */
void rf24_reset(RF24_Handle *rf, uint8_t reg)
{
    if (reg == STATUS_REG) {
    	uint8_t clr = (1<<TX_DS) | (1<<MAX_RT) | (1<<RX_DR);
    	rf24_write_reg(rf, STATUS_REG, &clr, 1);
    }
    else if (reg == FIFO_STATUS) {
        uint8_t reset_val = 0x11;
        rf24_write_reg(rf, FIFO_STATUS, &reset_val, ONE_BYTE);
    }
    else {
        printf("rf24_reset: others register reset\r\n");
        rf24_write_reg(rf, CONFIG_REG,    &(uint8_t){0x08}, ONE_BYTE);
        rf24_write_reg(rf, EN_AA,         &(uint8_t){0x3F}, ONE_BYTE);
        rf24_write_reg(rf, EN_RX_ADDR,    &(uint8_t){0x03}, ONE_BYTE);
        rf24_write_reg(rf, ADDR_WID,      &(uint8_t){0x03}, ONE_BYTE);
        rf24_write_reg(rf, SET_AUTO_RETRS,&(uint8_t){0x03}, ONE_BYTE);
        rf24_write_reg(rf, RF_CH,         &(uint8_t){0x02}, ONE_BYTE);
        rf24_write_reg(rf, RF_SETUP,      &(uint8_t){0x0E}, ONE_BYTE);
        rf24_write_reg(rf, STATUS_REG,    &(uint8_t){0x00}, ONE_BYTE);
        rf24_write_reg(rf, OBSERVE_TX,    &(uint8_t){0x00}, ONE_BYTE);
        rf24_write_reg(rf, RPD,           &(uint8_t){0x00}, ONE_BYTE);

        uint8_t rx_addr_p0_def[5];
        memcpy(rx_addr_p0_def, rf->pipe0_rx_addr, MAX_ADDRESS);
        rf24_write_reg(rf, RX_PIPE_ADDR_0, rx_addr_p0_def, MAX_ADDRESS);

        uint8_t rx_addr_p1_def[5];
        memcpy(rx_addr_p1_def, rf->pipe1_rx_addr, MAX_ADDRESS);
        rf24_write_reg(rf, RX_PIPE_ADDR_1, rx_addr_p1_def, MAX_ADDRESS);

        rf24_write_reg(rf, RX_PIPE_ADDR_2, &(uint8_t){0xC3}, ONE_BYTE);
        rf24_write_reg(rf, RX_PIPE_ADDR_3, &(uint8_t){0xC4}, ONE_BYTE);
        rf24_write_reg(rf, RX_PIPE_ADDR_4, &(uint8_t){0xC5}, ONE_BYTE);
        rf24_write_reg(rf, RX_PIPE_ADDR_5, &(uint8_t){0xC6}, ONE_BYTE);

        uint8_t tx_addr_def[5];
        memcpy(tx_addr_def, rf->tx_addr, MAX_ADDRESS);
        rf24_write_reg(rf, TX_ADDR, tx_addr_def, MAX_ADDRESS);

        rf24_write_reg(rf, RX_PW_P0, &(uint8_t){0x00}, ONE_BYTE);
        rf24_write_reg(rf, RX_PW_P1, &(uint8_t){0x00}, ONE_BYTE);
        rf24_write_reg(rf, RX_PW_P2, &(uint8_t){0x00}, ONE_BYTE);
        rf24_write_reg(rf, RX_PW_P3, &(uint8_t){0x00}, ONE_BYTE);
        rf24_write_reg(rf, RX_PW_P4, &(uint8_t){0x00}, ONE_BYTE);
        rf24_write_reg(rf, RX_PW_P5, &(uint8_t){0x00}, ONE_BYTE);

        rf24_write_reg(rf, FIFO_STATUS, &(uint8_t){0x11}, ONE_BYTE);
        rf24_write_reg(rf, DYNPD,       &(uint8_t){0x00}, ONE_BYTE);
        rf24_write_reg(rf, FEATURE,     &(uint8_t){0x00}, ONE_BYTE);
    }
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
    rf24_read_reg(rf, ADDR_WID, &check, ONE_BYTE);
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
    rf24_read_reg(rf, ADDR_WID, &check, ONE_BYTE);
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
