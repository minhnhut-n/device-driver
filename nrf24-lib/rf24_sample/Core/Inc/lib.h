#ifndef _LIB_H_
#define _LIB_H_

//set of nrf24 macros
#define CONFIG      0x00
#define EN_AA       0x01
#define EN_RXADDR   0x02
#define SETUP_AW    0x03
#define SETUP_RETR  0x04
#define RF_CH       0x05
#define RF_SETUP    0x06
#define STATUS      0x07
#define OBSERVE_TX  0x08
#define CD          0x09
#define RX_ADDR_P0  0x0A
#define RX_ADDR_P1  0x0B
#define RX_ADDR_P2  0x0C
#define RX_ADDR_P3  0x0D
#define RX_ADDR_P4  0x0E
#define RX_ADDR_P5  0x0F
#define TX_ADDR     0x10
#define RX_PW_P0    0x11
#define RX_PW_P1    0x12
#define RX_PW_P2    0x13
#define RX_PW_P3    0x14
#define RX_PW_P4    0x15
#define RX_PW_P5    0x16
#define FIFO_STATUS 0x17
#define DYNPD	    0x1C
#define FEATURE	    0x1D

/* Instruction Mnemonics */
#define R_REGISTER    0x00
#define W_REGISTER    0x20
#define REGISTER_MASK 0x1F
#define ACTIVATE      0x50
#define R_RX_PL_WID   0x60
#define R_RX_PAYLOAD  0x61
#define W_TX_PAYLOAD  0xA0
#define W_ACK_PAYLOAD 0xA8
#define FLUSH_TX      0xE1
#define FLUSH_RX      0xE2
#define REUSE_TX_PL   0xE3
#define NOP           0xFF

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

typedef enum {
    ARD_250US  = 0,
    ARD_500US  = 1,
    ARD_750US  = 2,
    ARD_1000US = 3
	//... it have 16 value from 250 to 4000us
} ard_delay_t;

/* User defined */
#define PAYLOAD_MAX 32
#define CMD_WRITE_TIME 100
#define DATA_WRITE_TIME 1000
#define MAX_ADDR_LEN    5
#define ENABLE_ALL_RX_PIPE 6

//nrf24 functions
void rf24_init(void);

void rf24_tx_mode(uint8_t *Address, uint8_t channel);
void rf24_rx_mode(uint8_t *addr, uint8_t channel);

uint8_t rf24_transmit(uint8_t *data);
uint8_t is_data_available(int pipenum);

void rf24_receive(uint8_t *data);
void rf24_read_all(uint8_t *data); // store all address value into 1 buffer

/**
 * @brief: CRC setting for payload transmit
 */
void rf24_crc_setting(uint8_t state, uint8_t numCRCByte);
void rf24_autoAck_enable(uint8_t type);
uint8_t rf24_autoAck_config(uint8_t ack_time, uint8_t ack_retry);
void rf24_enable_rx_pipe(uint8_t enable, uint8_t pipe_num);

#endif /* _LIB_H_ */
