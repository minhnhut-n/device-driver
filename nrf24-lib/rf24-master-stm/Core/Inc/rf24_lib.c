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
static void rf24_autoAck_enable(RF24_Handle *rf, uint8_t pipe)
{
    uint8_t config = 0;

    rf24_read_reg(rf, EN_AA, &config, ONE_BYTE);
    printf("Autoack EN_AA info: %02x\r\n", config);
    config |= (1 << pipe);
    rf24_write_reg(rf, EN_AA, &config, ONE_BYTE);
    printf("Autoack EN_AA after info: %02x\r\n", config);
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
void rf24_write_data(RF24_Handle *rf, const uint8_t* buffer, uint8_t size, uint8_t writeType)
{
    if (buffer == NULL) {
        printf("rf24_write_data: buffer is NULL -> abort\r\n");
        return;
    }

    if (!rf->dynamic_pay_load) {
        if (rf->payload_size == 0) {
            printf("rf24_write_data: payload_size == 0 (not initialized)\r\n");
            return;
        }
        size = minValue(size, rf->payload_size);
        // In fixed payload mode, ensure we write exactly payload_size bytes
        uint8_t pad[32] = {0}; // Max payload size is 32
        memcpy(pad, buffer, size);
        buffer = pad;
        size = rf->payload_size;
    } else {
        size = minValue(size, ONE_SECTION_BUF);
    }

    if (size == 0) {
        printf("rf24_write_data: size == 0 -> nothing to send\r\n");
        return;
    }

    //Transmission set
    spi_beginTransaction(rf);
    uint8_t cmd = W_PAY_LOAD;
    if (HAL_SPI_Transmit(rf->cfg.hspi, &cmd, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }

    if (HAL_SPI_Transmit(rf->cfg.hspi, buffer, size, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }

    rf24_ce_pin(rf, BIT_ENABLE);
    HAL_Delay(1);
    rf24_ce_pin(rf, BIT_DISABLE);
    spi_endTransaction(rf);
}

/**
 * @brief function support for reading user data wih spi
 */
void rf24_read_data(RF24_Handle *rf, uint8_t* buffer, uint8_t size)
{
    //dynamic payload check
    uint8_t EmptyBuffer = isEmptyBuffer(buffer);
    if (rf->dynamic_pay_load) {
        size = minValue(size, ONE_SECTION_BUF);
    }
    else {
        size = minValue(size, rf->payload_size);
        EmptyBuffer = rf->payload_size - size;
        printf("Free Buffer: %d\r\n", EmptyBuffer);
    }

    //receive action
    spi_beginTransaction(rf);
    uint8_t cmd = R_PAY_LOAD;
    uint8_t dummy = NOP;
    if (HAL_SPI_Transmit(rf->cfg.hspi, &cmd, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }

    if (HAL_SPI_TransmitReceive(rf->cfg.hspi, &dummy, buffer, size, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }
    spi_endTransaction(rf);
}


/**
 * @brief Using SETUP_AW to check it is value or not.
 * Value on this REG can be 1,2,3 respectively with 3,4,5 bytes
 * So the offset is = -2
 */
bool isValid_AddrWidth(RF24_Handle *rf)
{
    uint8_t rtn = 0;
    rf24_read_reg(rf, SET_ADDR_WID, &rtn, ONE_BYTE);

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
    config |= ((rf->power&0x03) << 1);

    rf24_write_reg(rf, RF_SETUP, &config, ONE_BYTE);
    printf("power consumtion RF_SETUP after info: %02x\r\n", config);
}

/**
 * @brief Channel set for rf24
 */
void rf24_channel_set(RF24_Handle *rf, uint8_t channel)
{
    uint8_t reset = 0;
    printf("Channel set RF_CH info: %02x\r\n", 0);
    rf24_read_reg(rf, RF_CH, &reset, ONE_BYTE);

    if(channel > 125 || channel < 0)
    {
        printf("Channel set invalid\r\n");
        return;
    }

    printf("Channel set RF_CH info: %02x\r\n", channel);
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
void rf24_power_set(RF24_Handle *rf, uint8_t status)
{
    uint8_t config = 0;
    rf24_read_reg(rf, CONFIG_REG, &config, ONE_BYTE);
    printf("Power set CONFIG info: %02x\r\n", config);

    if (status) {
        config |= (1 << 1);
    }
    else {
        config &= ~(1 << 1);
    }

    printf("Power set CONFIG after info: %02x\r\n", config);
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
    if (pipeNum == PIPE0 && rf->is_enable_payload_ack) {
        value = 0; // No payload in ACK for simplicity
    } else {
        value = rf->payload_size;
    }
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
    rf24_write_reg(rf, EN_RX_ADDR, &value, ONE_BYTE);
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
void rf24_rx_mode(RF24_Handle *rf)
{
    rf->cfg.rf24_config_reg |= (RX_MODE << PRIM_RX);
    rf24_write_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    //Clear tx/rx interrupt flag in STATUS REG <important>
    uint8_t irq_data = RF24_IRQ_ALL;
    rf24_write_reg(rf, STATUS_REG, &irq_data, ONE_BYTE);
    rf24_ce_pin(rf, BIT_ENABLE);
}

/**
 * @brief Configuration mode TX on RF24
 * @def reverse with rx mode
 */
void rf24_tx_mode(RF24_Handle *rf)
{
    rf->is_tx_mode = true;

    rf->cfg.rf24_config_reg = (rf->cfg.rf24_config_reg & ~(1 << PRIM_RX)) | (TX_MODE << PRIM_RX);
    rf24_write_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    //Clear tx/rx interrupt flag in STATUS REG <important>
    uint8_t irq_data = RF24_IRQ_ALL;
    rf24_write_reg(rf, STATUS_REG, &irq_data, ONE_BYTE);
//    rf24_ce_pin(rf, true);
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
 * @brief Start mode listening on RF24
 * @def when listening start, mode turn from TX -> RX mode
 * then device could you rf24_read to read value from rf24
 */
void rf24_listen_start(RF24_Handle *rf)
{
    rf24_rx_mode(rf);

    //logic for recovering addr on pipe0
    if (rf->is_restore_pipe0_addr) {
        rf24_write_reg(rf, RX_PIPE_ADDR_0, rf->pipe0_rx_addr, MAX_ADDRESS);
    }
    else {
        //this is close for ack data event, not receive user data at this time
        rf24_pipeData_rx_close(rf, PIPE0);
    }
}

/**
 * @brief Stop mode listening on RF24
 * @def this function mean, when listening is done, close 
 * section and return to default (standby mode)
 */
void rf24_listen_stop(RF24_Handle *rf)
{
    rf24_standby_mode(rf);
    HAL_Delay(1);
    
    //reset ack for tx flag
    if (rf->is_enable_payload_ack) {
        rf24_empty_tx_buffer(rf);
    }

    rf->cfg.rf24_config_reg &= ~(BIT_ENABLE << PRIM_RX);
    rf24_write_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);
}

/**
 * @brief Empty buffer TX
 */
void rf24_empty_tx_buffer(RF24_Handle *rf)
{
    spi_beginTransaction(rf);
    uint8_t command = FLUSH_TX;
    HAL_SPI_Transmit(rf->cfg.hspi, &command, ONE_BYTE, 2);
    spi_endTransaction(rf);
}

/**
 * @brief Empty buffer RX
 */
void rf24_empty_rx_buffer(RF24_Handle *rf)
{
    spi_beginTransaction(rf);
    uint8_t command = FLUSH_RX;
    HAL_SPI_Transmit(rf->cfg.hspi, &command, ONE_BYTE, 2);
    spi_endTransaction(rf);
}

/**
 * @brief Init RF24 module
 */
uint8_t rf24_init(RF24_Handle *rf)
{
    printf("\n====  INIT RF24   ====\r\n");

    rf24_standby_mode(rf);

    rf24_autoAck_enable(rf, rf->pipe_auto_ack);

    rf24_powerConsumption_set(rf);

    //    uint8_t aw_reg = rf->addr_len - 2;
    uint8_t aw_reg = MAX_ADDRESS - 2;
    rf24_write_reg(rf, SET_ADDR_WID, &aw_reg, ONE_BYTE);
    printf("Address setup info: %02x\r\n", aw_reg);
    rf24_read_reg(rf, SET_ADDR_WID, &aw_reg, ONE_BYTE);
    printf("Address setup after info: %02x\r\n", aw_reg);

    rf24_channel_set(rf, rf->channel);

    rf24_baudrate_set(rf, rf->baudrate);

    printf("====  END INIT RF24   ====\r\n");

    return  0;
}

/**
 * =============================================================================
 * FOR DEBUG WITH SERIAL LOG ONLY
 * =============================================================================
 */

void print_tc_function(RF24_Handle *rf, uint8_t pipeNum) {
    //check pipe open/close
    printf("\r\ncheck pipe open/close =======\r\n");
    uint8_t en_bit_1 = 0;
    uint8_t en_bit_2 = 0;
    const uint8_t *str = (const uint8_t *)"nhutn";
    uint8_t buffer1[MAX_ADDRESS] = "nhutn";
    uint8_t buffer2[MAX_ADDRESS] = {0};
    uint8_t targetPipeAddr = pipeAddr[PIPE0];
    
    rf24_read_reg(rf, EN_RX_ADDR, &en_bit_1, ONE_BYTE);
    rf24_pipeData_rx_open(rf, PIPE0, buffer1);
    rf24_read_reg(rf, EN_RX_ADDR, &en_bit_2, ONE_BYTE);
    rf24_read_reg(rf, targetPipeAddr, buffer2, MAX_ADDRESS);

    if (en_bit_1 != en_bit_2) printf("TRUE!(%d)\r\n", __LINE__);
    else printf("FALSE!(%d)\r\n", __LINE__);

    if (!memcmp(buffer1, buffer2, MAX_ADDRESS)) printf("TRUE!(%d)\r\n", __LINE__);
    else printf("FALSE!(%d)\r\n", __LINE__);

    rf24_pipeData_rx_close(rf, PIPE0);
    rf24_read_reg(rf, EN_RX_ADDR, &en_bit_1, ONE_BYTE);
    if (en_bit_1 != en_bit_2) printf("TRUE!(%d)\r\n", __LINE__);
    else printf("FALSE!(%d)\r\n", __LINE__);

    //check TX buffer
    printf("check tx buffer =======\r\n");
    rf24_pipeData_tx_registry(rf, str);
    rf24_read_reg(rf, pipeAddr[PIPE0], buffer2, MAX_ADDRESS);
    rf24_read_reg(rf, TX_ADDR, buffer1, MAX_ADDRESS);
    if (!memcmp(buffer1, buffer2, MAX_ADDRESS) && !memcmp(buffer1, (const uint8_t*)str, MAX_ADDRESS))
        printf("TRUE!(%d)\r\n", __LINE__);
    else 
        printf("FALSE!(%d)\r\n", __LINE__);

    //check rx/tx mode switch
    printf("check tx/rx switch mode =======\r\n");
    rf24_read_reg(rf, CONFIG_REG, &en_bit_1, ONE_BYTE);
    printf("config reg data: %02x\r\n", en_bit_1);
    rf24_read_reg(rf, STATUS_REG, &en_bit_2, ONE_BYTE);
    printf("status reg data: %02x\r\n", en_bit_2);
    if (en_bit_1 & (RX_MODE << PRIM_RX))
        rf24_tx_mode(rf);
    else
        rf24_rx_mode(rf);
    
    rf24_read_reg(rf, CONFIG_REG, &en_bit_1, ONE_BYTE);
    printf("after config reg data: %02x\r\n", en_bit_1);
    rf24_read_reg(rf, STATUS_REG, &en_bit_2, ONE_BYTE);
    printf("after status reg data: %02x\r\n", en_bit_2);

    //check standby mode
    printf("check standby mode =======\r\n");
    rf24_read_reg(rf, CONFIG_REG, &en_bit_1, ONE_BYTE);
    printf("standby mode: %02x\r\n", en_bit_1);

    //check listening start/stop
    printf("check listen start/stop mode =======\r\n");
    rf24_listen_start(rf);
    rf24_read_reg(rf, CONFIG_REG, &en_bit_1, ONE_BYTE);
    printf("config reg data: %02x\r\n", en_bit_1);
    rf24_read_reg(rf, STATUS_REG, &en_bit_2, ONE_BYTE);
    printf("status reg data: %02x\r\n", en_bit_2);
    rf24_listen_stop(rf);
    rf24_read_reg(rf, CONFIG_REG, &en_bit_1, ONE_BYTE);
    printf("after config reg data: %02x\r\n", en_bit_1);
    rf24_read_reg(rf, STATUS_REG, &en_bit_2, ONE_BYTE);
    printf("after status reg data: %02x\r\n", en_bit_2);
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
    rf24_read_reg(rf, SET_ADDR_WID, &check, ONE_BYTE);
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
