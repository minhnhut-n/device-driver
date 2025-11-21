/*
 * rf24_info_reg.h
 *
 *  Created on: Nov 20, 2025
 *      Author: Aelius_Nguyen
 */

#ifndef INC_RF24_INFO_REG_H_
#define INC_RF24_INFO_REG_H_

/*
 * Command for RF24, SPI protocol
 *
 * Command word: MSB -> LSB
 * Data: LSB --> MSB
 */
#define R_REG			0x00
#define W_REG			0x20

#define R_PAY_LOAD		0x61
#define W_PAY_LOAD		0xA0

#define FLUSH_TX		0xE1 //Used to throw out data
#define FLUSH_RX		0xE2 //Used to receive data

#define W_ACK_PAYLOAD	0xA8 //Used with ack check with frame (if use)
#define W_NO_ACK_PAYLD	0xB0 //Same but no ack in frame

#define NOP				0xFF //None of operation
#define R_RX_PL_WID		0x70 //Read RX width on top.

/*
 * Configuration register
 */
#define CONFIG_REG		0x00
#define EN_AUTO_ACK		0x01
#define EN_RX_ADDR		0x02 //for special pipe to communication
#define SET_ADDR_WID	0x03
#define SET_AUTO_RETRS	0x04
#define SET_FREQ_CHA	0x05
#define SET_REG_RATE	0x06
#define STATUS_REG		0x07
#define OBSERVE_TX		0x08
//
#define RX_PIPE_ADDR_0	0x0A
#define RX_PIPE_ADDR_1	0x0B
#define RX_PIPE_ADDR_2	0x0C
#define RX_PIPE_ADDR_3	0x0D
#define RX_PIPE_ADDR_4	0x0E
#define RX_PIPE_ADDR_5	0x0F
#define TX_ADDR			0x10 // Use for transmit device (master only)
#define RX_PW_P0		0x11
#define RX_PW_P1        0x12
#define RX_PW_P2		0x13
#define RX_PW_P3		0x14
#define RX_PW_P4		0x15
#define RX_PW_P5		0x16
#define FIFO_STATUS     0x17

//User config
#define W_DATA			1
#define W_CONFIG		0
#define RX_MODE			1
#define TX_MODE			0
#define MAX_FRAME_DATA	5
#define RF_SPI_TIMEOUT	15
#define RX_PIPE_NULL_0	0

#endif /* INC_RF24_INFO_REG_H_ */
