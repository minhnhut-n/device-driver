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

#define BIT_ENABLE			1
#define BIT_DISABLE			0

#define R_REG			0x00
#define W_REG			0x20

#define R_PAY_LOAD		0x61
#define W_PAY_LOAD		0xA0

#define FLUSH_TX		0xE1 //Used to throw out data
#define FLUSH_RX		0xE2 //Used to receive data

#define W_TX_PAYLOAD	0xA0 //Used with ack check with frame (if use)
#define W_TX_PAYLOAD_NOACK	0xB0 //Same but no ack in frame

#define NOP				0xFF //None of operation
#define R_RX_PL_WID		0x70 //Read RX width on top.

/*
 * Configuration register
 */
#define CONFIG_REG		0x00
#define EN_AA   		0x01
#define EN_RXADDR		0x02 //for special pipe to communication
#define SETUP_AW    	0x03
#define SETUP_RETR  	0x04
#define RF_CH			0x05
#define RF_SETUP    	0x06
#define STATUS_REG		0x07
#define OBSERVE_TX		0x08
#define RPD             0x09
#define TX_ADDR			0x10
#define RX_PW_P0		0x11
#define RX_PW_P1        0x12
#define RX_PW_P2		0x13
#define RX_PW_P3		0x14
#define RX_PW_P4		0x15
#define RX_PW_P5		0x16
#define FIFO_STATUS     0x17
#define DYNPD           0x1C
#define FEATURE         0x1D
#define ONE_SECTION_BUF 32

/* Bit Mnemonics */
#define MASK_RX_DR  6
#define MASK_TX_DS  5
#define MASK_MAX_RT 4
#define EN_CRC      3
#define CRCO        2
#define PWR_UP      1
#define PRIM_RX     0
#define ENAA_P5     5
#define ENAA_P4     4
#define ENAA_P3     3
#define ENAA_P2     2
#define ENAA_P1     1
#define ENAA_P0     0
#define ERX_P5      5
#define ERX_P4      4
#define ERX_P3      3
#define ERX_P2      2
#define ERX_P1      1
#define ERX_P0      0
#define AW          0
#define ARD         4
#define ARC         0
#define PLL_LOCK    4
#define CONT_WAVE   7
#define RF_DR_HIGH  3
#define RF_DR_LOW	5
#define RF_PWR      6
#define RX_DR       6
#define TX_DS       5
#define MAX_RT      4
#define RX_P_NO     1
#define TX_FULL     5
#define PLOS_CNT    4
#define ARC_CNT     0
#define TX_REUSE    6
#define FIFO_FULL   5
#define TX_EMPTY    4
#define RX_FULL     1
#define RX_EMPTY    0
#define DPL_P5      5
#define DPL_P4      4
#define DPL_P3      3
#define DPL_P2      2
#define DPL_P1      1
#define DPL_P0      0
#define EN_DPL      2
#define EN_ACK_PAY  1
#define EN_DYN_ACK  0

//User config
#define W_DATA			1
#define W_CONFIG		0
#define RX_MODE			1
#define TX_MODE			0
#define MAX_ADDRESS 	5
#define RF_SPI_TIMEOUT	200
#define RX_PIPE_NULL_0	0
#define ONE_BYTE        1
#define ADDR_WD_OFFSET  2
#define V_RX_DR         64
#define V_TX_DS         32
#define V_MAX_RT        16
#define SPI_TIMEOUT     200

#define PIPE0 0
#define PIPE1 1
#define PIPE2 2
#define PIPE3 3
#define PIPE4 4
#define PIPE5 5

#define BAUD_1MBPS 0
#define BAUD_2MBPS 1
#define BAUD_250KB 2

#define MIN_POWER 	 0
#define MID_LO_POWER 1
#define	MID_HI_POWER 2
#define MAX_POWER 	 3

#define RE_ACK_TIME  250	  //microseconds
#define RE_ACK_COUNT 5    //max re-ack count

typedef enum {
    RX_PIPE_ADDR_0 =	0x0A,
    RX_PIPE_ADDR_1 =	0x0B,
    RX_PIPE_ADDR_2 =	0x0C,
    RX_PIPE_ADDR_3 =	0x0D,
    RX_PIPE_ADDR_4 =	0x0E,
    RX_PIPE_ADDR_5 =	0x0F
}pipe_enum_t;

typedef enum {
    ADDR_3_BYTE =	1,
    ADDR_4_BYTE =	2,
    ADDR_5_BYTE =	3
}addr_width_enum_t;


#endif /* INC_RF24_INFO_REG_H_ */
