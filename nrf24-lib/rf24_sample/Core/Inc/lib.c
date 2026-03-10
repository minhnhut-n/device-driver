#include "stm32f1xx_hal.h"
#include "lib.h"
#include "string.h"

//modifiable declare
extern SPI_HandleTypeDef hspi2;
#define RF24_SPI &hspi2
#define RF24_CE_PIN GPIO_PIN_12
#define RF24_CE_PORT GPIOB
#define RF24_CS_PIN GPIO_PIN_8
#define RF24_CS_PORT GPIOA


//function, not expect to change if not have issue
#define CS_SELECT() \
    HAL_GPIO_WritePin(RF24_CS_PORT, RF24_CS_PIN, RESET);
#define CS_UNSELECT() \
    HAL_GPIO_WritePin(RF24_CS_PORT, RF24_CS_PIN, SET);
#define CE_ENABLE() \
    HAL_GPIO_WritePin(RF24_CE_PORT, RF24_CE_PIN, SET);
#define CE_DISABLE() \
    HAL_GPIO_WritePin(RF24_CE_PORT, RF24_CE_PIN, RESET);

//core function
void rf24_write_reg(uint8_t reg, uint8_t data) {
    uint8_t buf[2];
    buf[0] = W_REGISTER|reg;
    buf[1] = data;

    CS_SELECT();
    HAL_SPI_Transmit(RF24_SPI, buf, 2, DATA_WRITE_TIME);
    CS_UNSELECT();
}
void rf24_write_regMulti(uint8_t reg, uint8_t *data, uint8_t sizeOfData) {
    uint8_t cmd = W_REGISTER|reg;

    CS_SELECT();
    HAL_SPI_Transmit(RF24_SPI, &cmd, 1, CMD_WRITE_TIME);
    HAL_SPI_Transmit(RF24_SPI, data, sizeOfData, DATA_WRITE_TIME);
    CS_UNSELECT();
}

uint8_t rf24_read_reg(uint8_t reg) {
    uint8_t cmd = R_REGISTER|reg;
    uint8_t data = 0;

    CS_SELECT();
    HAL_SPI_Transmit(RF24_SPI, &cmd, 1, CMD_WRITE_TIME);
    HAL_SPI_Receive(RF24_SPI, &data, 1, DATA_WRITE_TIME);
    CS_UNSELECT();
    
    return data;
}
void rf24_read_regMulti(uint8_t reg, uint8_t *buf, uint8_t size) {
    uint8_t cmd = R_REGISTER|reg;

    CS_SELECT();
    HAL_SPI_Transmit(RF24_SPI, &cmd, 1, CMD_WRITE_TIME);
    HAL_SPI_Receive(RF24_SPI, buf, size, DATA_WRITE_TIME);
    CS_UNSELECT();
}

//api layer
void rf24_reset(uint8_t reg) {
    switch(reg) {
    case STATUS:
        rf24_write_reg(STATUS, 0x00);
        break;
    case FIFO_STATUS:
        rf24_write_reg(FIFO_STATUS, 0x11);
        break;
    default:
        rf24_write_reg(CONFIG, 0x08);
        rf24_write_reg(EN_AA, 0x3F);
        rf24_write_reg(EN_RXADDR, 0x03);
        rf24_write_reg(SETUP_AW, 0x03);
        rf24_write_reg(SETUP_RETR, 0x03);
        rf24_write_reg(RF_CH, 0x02);
        rf24_write_reg(RF_SETUP, 0x0E);
        rf24_write_reg(STATUS, 0x00);
        rf24_write_reg(OBSERVE_TX, 0x00);
        rf24_write_reg(CD, 0x00);

        uint8_t rx_addr_p0_def[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};
        rf24_write_regMulti(RX_ADDR_P0, rx_addr_p0_def, 5);
        uint8_t rx_addr_p1_def[5] = {0xC2, 0xC2, 0xC2, 0xC2, 0xC2};
        rf24_write_regMulti(RX_ADDR_P1, rx_addr_p1_def, 5);
        
        rf24_write_reg(RX_ADDR_P2, 0xC3);
        rf24_write_reg(RX_ADDR_P3, 0xC4);
        rf24_write_reg(RX_ADDR_P4, 0xC5);
        rf24_write_reg(RX_ADDR_P5, 0xC6);
        
        uint8_t tx_addr_def[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};
        rf24_write_regMulti(TX_ADDR, tx_addr_def, 5);
        
        rf24_write_reg(RX_PW_P0, 0);
        rf24_write_reg(RX_PW_P1, 0);
        rf24_write_reg(RX_PW_P2, 0);
        rf24_write_reg(RX_PW_P3, 0);
        rf24_write_reg(RX_PW_P4, 0);
        rf24_write_reg(RX_PW_P5, 0);
        rf24_write_reg(FIFO_STATUS, 0x11);
        rf24_write_reg(DYNPD, 0);
        rf24_write_reg(FEATURE, 0);
        break;
    }
}

void rf24_init(void) {
    //configuring condition
    CE_DISABLE();
    
    rf24_reset(0);
    rf24_write_reg(CONFIG, 0);

    rf24_autoAck_enable(0);
    rf24_enable_rx_pipe(0, 0);
    rf24_write_reg(SETUP_AW, 0x03); //five bytes address as default
    rf24_write_reg(SETUP_RETR, 0);  //no retry on auto-ack
    rf24_write_reg(RF_CH, 12);
    rf24_write_reg(RF_SETUP, 0);    // 1Mbps, 0dBm
    
    //end config
    CE_ENABLE();
}

void rf24_tx_mode(uint8_t *Address, uint8_t channel) {
    CE_DISABLE();

    rf24_write_reg(RF_CH, channel);
    rf24_write_regMulti(TX_ADDR, Address, MAX_ADDR_LEN);

    //change mode and power up device
    uint8_t config = rf24_read_reg(CONFIG);
    config = (config|(1<<1)) & ~(1<<0);
    rf24_write_reg(CONFIG, config);

    CE_ENABLE();
}

void rf24_sendCmd(uint8_t cmd) {
    CS_SELECT();
    HAL_SPI_Transmit(RF24_SPI, &cmd, 1, CMD_WRITE_TIME);
    CS_UNSELECT();
}

uint8_t rf24_transmit(uint8_t *data) {
    uint8_t cmd = 0;
    CS_SELECT();

    // write payload
    cmd = W_TX_PAYLOAD;
    HAL_SPI_Transmit(RF24_SPI, &cmd, 1, CMD_WRITE_TIME);
    HAL_SPI_Transmit(RF24_SPI, data, PAYLOAD_MAX, DATA_WRITE_TIME);

    CS_UNSELECT();
    HAL_Delay(1); //for pin to settle

    //write with no ack so we just check right away
    uint8_t fifo_reg = rf24_read_reg(FIFO_STATUS);
    uint8_t txBuf_full = fifo_reg & (1<<4);
    uint8_t device_disconnected = fifo_reg & (1<<3);
    if (txBuf_full && !device_disconnected) {
        //clear buffer reg
        cmd = FLUSH_TX;
        rf24_sendCmd(cmd);

        //reset fifo status register
        rf24_reset(FIFO_STATUS);
        return 1;
        /*fifo can not auto~ delete, reset action need when it done*/
    }
    /*assert as fail*/
    return 0;
}

void rf24_rx_mode(uint8_t *addr, uint8_t channel ) {
    //disable before config
    CE_DISABLE();

    uint8_t addrBuf[MAX_ADDR_LEN];
    memset(addrBuf, 0, MAX_ADDR_LEN);
    memcpy(addrBuf, addr, MAX_ADDR_LEN);

    //to clear all flag history
    rf24_reset(STATUS);
    rf24_write_reg(RF_CH, channel);

    //enable pipe for rx, with data on pipe 1
    uint8_t en_rxaddr = rf24_read_reg(EN_RXADDR);
    en_rxaddr |= (1<<1);
    rf24_write_reg(EN_RXADDR, en_rxaddr);

    /* we know this is pipe1 address : xxxxxxxxx(x)
    The LSB bit is the only thing change from address of
    pipe 1 -> 5.

    That mean all previous bits, others pipe (from pipe2->) is "copy"
    correctly from pipe1
    */
    rf24_write_regMulti(RX_ADDR_P1, addrBuf, MAX_ADDR_LEN);
    // rf24_write_reg(RX_ADDR_P2, addrBuf[0]); // pipe2 LSB must match the TX address LSB
    rf24_write_reg(RX_PW_P1, PAYLOAD_MAX); // payload setting pipe1
    // rf24_write_reg(RX_PW_P2, PAYLOAD_MAX); // payload setting pipe2

    uint8_t config = rf24_read_reg(CONFIG);
    //change mode to rx and power up
    config = (1 << 0) | (1 << 1);
    rf24_write_reg(CONFIG, config);

    CE_ENABLE();
}

uint8_t is_data_available(int pipenum) {
    uint8_t status = rf24_read_reg(STATUS);
    uint8_t data_in_rxBuf = (status & (1<<6)) != 0;  // RX_DR
    uint8_t active_pipe = (status >> 1) & 0x07;      // RX_P_NO bits[3:1]
    uint8_t pipe_available = (active_pipe == (uint8_t)pipenum);

    if (data_in_rxBuf && pipe_available) {
        //clear bit for next trigger (new data)
        rf24_write_reg(STATUS, (1<<6));
        return 1;
    }
    /*assert as fail*/
    return 0;
}

void rf24_receive(uint8_t *data) {
    uint8_t cmd = 0;
    CS_SELECT();

    cmd = R_RX_PAYLOAD;
    HAL_SPI_Transmit(RF24_SPI, &cmd, 1, CMD_WRITE_TIME);
    HAL_SPI_Receive(RF24_SPI, data, PAYLOAD_MAX, DATA_WRITE_TIME);

    CS_UNSELECT();
    //clear after get data
    cmd = FLUSH_RX;
    rf24_sendCmd(cmd);
}

// Read all the Register data
void rf24_read_all (uint8_t *data)
{
	for (int i=0; i<10; i++)
	{
		*(data+i) = rf24_read_reg(i);
	}

	rf24_read_regMulti(RX_ADDR_P0, (data+10), 5);

	rf24_read_regMulti(RX_ADDR_P1, (data+15), 5);

	*(data+20) = rf24_read_reg(RX_ADDR_P2);
	*(data+21) = rf24_read_reg(RX_ADDR_P3);
	*(data+22) = rf24_read_reg(RX_ADDR_P4);
	*(data+23) = rf24_read_reg(RX_ADDR_P5);

	rf24_read_regMulti(TX_ADDR, (data+24), 5);

	for (int i=29; i<38; i++)
	{
		*(data+i) = rf24_read_reg(i-12);
	}

}

/**
 * @brief: CRC setting for payload transmit
 */
void rf24_crc_setting(uint8_t state, uint8_t numCRCByte) {
    uint8_t config = rf24_read_reg(CONFIG);
    if (state) {
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
    rf24_write_reg(CONFIG, config);
}

/**
 * @version 0.1
 * @brief rf24_autoAck_enable
 * @author minhnhut-n
 * @function: auto-ack for 2 ways verifing and sending data (high reliable) 
 */
void rf24_autoAck_enable(uint8_t type)
{
    uint8_t config = rf24_read_reg(CONFIG);
    if (type) //true
    {
        config = 0x3F; //enable all pipe
    }
    else
    {
        config = 0x00; //disable all pipe
    }
    rf24_write_reg(EN_AA, config);
}
/**
 * @version 0
 * @brief rf24_autoAck_config
 * @author minhnhut-n
 * @function: configuration for autoack if it is enabled
 * setting about 2 parameters:
 * ack_time: time, which is a gap between 2 consecutive send
 * 250 - 4000us
 * ack_retry: number of times resend (no ack is received)
 */
uint8_t rf24_autoAck_config(uint8_t ack_time, uint8_t ack_retry)
{  
    if (ack_time < 0) {
        return 0;
    }

    uint8_t config = 0x00;  
    config |= (ack_time) << 4 | (ack_retry << 0);
    rf24_write_reg(SETUP_RETR, config);
    return 1;
}
/**
 * @brief: for open specific pipe for reading message
 */
void rf24_enable_rx_pipe(uint8_t enable, uint8_t pipe_num) {
    if (enable) {
        if (pipe_num != ENABLE_ALL_RX_PIPE) {    
            uint8_t reg_enable_aa = rf24_read_reg(EN_RXADDR);
            reg_enable_aa |= (1 << pipe_num);
            rf24_write_reg(EN_RXADDR, reg_enable_aa);
        }
        else {
            rf24_write_reg(EN_RXADDR, 0x3F);
        }
    }
    else {
        //disable
        rf24_write_reg(EN_RXADDR, 0x00);
    }
}
