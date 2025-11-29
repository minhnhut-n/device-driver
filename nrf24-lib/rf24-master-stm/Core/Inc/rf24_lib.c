/*
 * rf24_lib.c
 *
 *  Created on: Nov 20, 2025
 *      Author: Aelius_Nguyen
 */

#include "rf24_lib.h"

/**
 * Macro for define
 */

/**
 * Macro function
 */
#define isEmptyBuffer(buf)      ((buf) != NULL ? 0 : 1)
#define minValue(val1, val2)	((val1) < (val2) ? (val1) : (val2))

/**
 * Static function
 */
static void rf24_autoAck_enable(RF24_Handle *rf, uint8_t pipe)
{
	uint8_t config = 0;

	rf24_read_reg(rf, EN_AA, &config, ONE_BYTE);
	config |= (1 << pipe);
	rf24_write_reg(rf, EN_AA, &config, ONE_BYTE);
}
static void rf24_address_set(RF24_Handle *rf)
{
	uint8_t pipeChose = PIPE0;
	switch (rf->pipe)
	{
	case PIPE0:
		pipeChose = RX_PIPE_ADDR_0;
		break;
	case PIPE1:
		pipeChose = RX_PIPE_ADDR_1;
		break;
	case PIPE2:
		pipeChose = RX_PIPE_ADDR_2;
		break;
	case PIPE3:
		pipeChose = RX_PIPE_ADDR_3;
		break;
	case PIPE4:
		pipeChose = RX_PIPE_ADDR_4;
		break;
	case PIPE5:
		pipeChose = RX_PIPE_ADDR_5;
		break;
	}
	rf24_write_reg(rf, pipeChose, rf->address, rf->addr_len);
}


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
 * @brief function for configuration rf24 struct
 */
void rf24_hw_config(RF24_Handle *rf, uint8_t ce_pin, GPIO_TypeDef *ce_port, uint8_t csn_pin,  GPIO_TypeDef *csn_port,\
                    uint8_t rf_channel, uint8_t baudrate, uint8_t* addr, uint8_t _pipe, bool payLoadCondfig)
{
    rf->cfg.csnPort = csn_port;
    rf->cfg.cePort = ce_port;
    rf->cfg.csnPin = csn_pin;
    rf->cfg.cePin = ce_pin;

    rf->cfg.ce_status = false;

    rf->channel = rf_channel;
    rf->baudrate = baudrate;
    memcpy(rf->address, addr, MAX_ADDRESS);

    uint8_t lenOfAddr = sizeof(rf->address)/ sizeof(rf->address[0]);
    lenOfAddr = minValue(lenOfAddr, MAX_ADDRESS);
    rf->addr_len = lenOfAddr;
    rf->pipe = _pipe;
    rf->dynamic_pay_load = payLoadCondfig;
    rf->is_restore_pipe0_addr = true;
}
/**
 * @brief function support for writbg configuration wih spi
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
 * @brief function support for writing user data wih spi
 */
void rf24_write_data(RF24_Handle *rf, const uint8_t* buffer, uint8_t size, uint8_t writeType)
{
    //dynamic payload check
    uint8_t EmptyBuffer = isEmptyBuffer(buffer);
    if (rf->dynamic_pay_load) {
        size = minValue(size, ONE_SECTION_BUF);
    }
    else {
        size = minValue(size, rf->payload_size);
        EmptyBuffer = rf->payload_size - size;
    }
    printf("Free Buffer: %d\r\n", EmptyBuffer);

    //Transmittion set
    spi_beginTransaction(rf);
    uint8_t cmd = W_PAY_LOAD;
    if (HAL_SPI_Transmit(rf->cfg.hspi, &cmd, ONE_BYTE, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }

    if (HAL_SPI_Transmit(rf->cfg.hspi, buffer, size, RF_SPI_TIMEOUT) != HAL_OK) {
        printf("Error when write data %02X \r\n", cmd);
    }
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
    }
    printf("Free Buffer: %d\r\n", EmptyBuffer);

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
 * =========================
 * This function is for RF24
 * =========================
 */

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
    if (rtn > 2 && rtn < 6)
    {
        return true;
    }
    
    return false;
}

/**
 * @brief change pin CE logic and status
 */
void rf24_ce_pin(RF24_Handle *rf, bool status)
{
    if (status)
    {
        HAL_GPIO_WritePin(rf->cfg.cePort, rf->cfg.cePin, 1);
        rf->cfg.ce_status = true;
    }
    else
    {
        HAL_GPIO_WritePin(rf->cfg.cePort, rf->cfg.cePin, 0);
        rf->cfg.ce_status = false;
    }
}

/**
 * @brief Configuration mode RX on RF24
 */
void rf24_rx_mode(RF24_Handle *rf)
{
    rf->cfg.rf24_config_reg |= (RX_MODE << PRIM_RX);
    rf24_write_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    //Clear tx/rx interrupt flag in STATUS REG
    uint8_t irq_data = RF24_IRQ_ALL;
    rf24_write_reg(rf, STATUS_REG, &irq_data, ONE_BYTE);
    rf24_ce_pin(rf, true);
}
/**
 * @brief Configuration mode TX on RF24
 */
void rf24_tx_mode(RF24_Handle *rf)
{
	rf->cfg.rf24_config_reg = (rf->cfg.rf24_config_reg & ~(1 << PRIM_RX)) | (TX_MODE << PRIM_RX);
    rf24_write_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    //Clear tx/rx interrupt flag in STATUS REG
    uint8_t irq_data = RF24_IRQ_ALL;
    rf24_write_reg(rf, STATUS_REG, &irq_data, ONE_BYTE);
    rf24_ce_pin(rf, true);
}
/**
 * @brief Configuration mode STANDBY on RF24
 */
void rf24_standby_mode(RF24_Handle *rf)
{
    rf24_ce_pin(rf, false);
    rf24_read_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);

    if ( !(rf->cfg.rf24_config_reg & (1 << PWR_UP)) ) {
        rf->cfg.rf24_config_reg |= (1 << PWR_UP);
        rf24_write_reg(rf, CONFIG_REG, &rf->cfg.rf24_config_reg, ONE_BYTE);
    }
    HAL_Delay(1);
}

/**
 * Pipe data set
 */
void rf24_pipeData_open(RF24_Handle *rf, uint8_t pipeNum)
{
    uint8_t dataReg = 0x00;
    rf24_read_reg(rf, EN_RX_ADDR, &dataReg, ONE_BYTE);
    // dataReg = ~(~dataReg | (1<<pipeNum));
    dataReg |= (1 << pipeNum);

    rf24_write_reg(rf, EN_RX_ADDR, &dataReg, ONE_BYTE);

    if (pipeNum == 0) {
        rf->is_restore_pipe0_addr = true;
    }
}
/**
 * Pipe data close
 */
void rf24_pipeData_close(RF24_Handle *rf, uint8_t pipeNum)
{
    uint8_t dataReg = 0x00;
    rf24_read_reg(rf, EN_RX_ADDR, &dataReg, ONE_BYTE);
    // dataReg = ~(~dataReg | (1<<pipeNum));
    dataReg &= ~(1 << pipeNum);

    rf24_write_reg(rf, EN_RX_ADDR, &dataReg, ONE_BYTE);

    if (pipeNum != 0) {
        rf->is_restore_pipe0_addr = false;
    }
}

/**
 * @brief Start mode listening on RF24
 */
void rf24_listen_start(RF24_Handle *rf)
{
    rf24_rx_mode(rf);

    //logic for recovering addr on pipe0
    if (rf->is_restore_pipe0_addr) {
        rf24_write_reg(rf, RX_PIPE_ADDR_0, rf->pipe0_rx_addr, rf->addr_len);
    }
    else {
        //this is close for ack data event, not receive user data at this time
        rf24_pipeData_close(rf, PIPE0);
    }
}
/**
 * @brief Stop mode listening on RF24
 */
void rf24_listen_stop(RF24_Handle *rf)
{
    rf24_standby_mode(rf);
    HAL_Delay(1);
    
    //reset ack for tx flag
    if (rf->is_enable_payload_ack) {
        rf24_empty_tx_buffer(rf);
    }

    rf->cfg.rf24_config_reg &= ~(1 << PRIM_RX);
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

	printf("==>> Standby mode <<==\r\n");
	rf24_standby_mode(rf);

	printf("==>> AutoACK mode <<==\r\n");
	rf24_autoAck_enable(rf, rf->pipe);

	printf("==>> Pipe open <<==\r\n");
	rf24_pipeData_open(rf, rf->pipe);

	printf("==>> Address config <<==\r\n");
	uint8_t aw_reg = rf->addr_len - 2;
	rf24_write_reg(rf, SET_ADDR_WID, &aw_reg, ONE_BYTE);

	printf("==>> Channel config <<==\r\n");
	rf24_write_reg(rf, SET_FREQ_CHA, &rf->channel, ONE_BYTE);

	printf("==>> Baudrate config <<==\r\n");
	rf24_write_reg(rf, RF_SETUP, &rf->baudrate, ONE_BYTE);

	printf("==>> Pipe Address	<<==\r\n");
	rf24_address_set(rf);

	printf("====  END INIT RF24   ====\r\n");

	return  0;
}

/*
 * DEBUG FUNCTION
 */
void print_state_init(RF24_Handle *rf)
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
	rf24_read_reg(rf, SET_FREQ_CHA, &check, ONE_BYTE);
	printf("Value: %02X\r\n", check);

	printf("==>> Baudrate config <<==\r\n");
	check = 0;
	rf24_read_reg(rf, RF_SETUP, &check, ONE_BYTE);
	printf("Value: %02X\r\n", check);

	printf("==>> Pipe Address	<<==\r\n");
	uint8_t buff[rf->addr_len +1];
	uint8_t pipeChose = PIPE0;
	switch (rf->pipe)
	{
	case PIPE0:
		pipeChose = RX_PIPE_ADDR_0;
		break;
	case PIPE1:
		pipeChose = RX_PIPE_ADDR_1;
		break;
	case PIPE2:
		pipeChose = RX_PIPE_ADDR_2;
		break;
	case PIPE3:
		pipeChose = RX_PIPE_ADDR_3;
		break;
	case PIPE4:
		pipeChose = RX_PIPE_ADDR_4;
		break;
	case PIPE5:
		pipeChose = RX_PIPE_ADDR_5;
		break;
	}
	rf24_read_reg(rf, pipeChose, buff, rf->addr_len);
	for (int i = 0; i <  rf->addr_len; i++)
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
