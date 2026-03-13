# NRF24 Driver for STM32F1x

This project provides an nRF24L01+ driver for STM32 (HAL-based). The popular Arduino RF24 library does not map directly to STM32 HAL, so this library was written to learn the nRF24 register model and to support larger STM32 projects.

## Features
- Read/write registers correctly (sample test included)
- Communicates with Arduino or other devices using RF24
- Auto-ack and CRC enabled for more reliable data transfer
- Version 0.1 (early stage)

## Installation

### Hardware Setup
Make sure the hardware wiring and power are solid before debugging the code. A noisy 3.3V rail or poor routing can corrupt packets and make the software look wrong.
My work is mess up for long time to understand what happened on my tranmition until I know that due to my "trash" route of signal.

![Test board](../../docs/test_board.png)
![Overview](../../docs/Overview.png)

### Software

#### Directory Tree
1. Clone the project:
   - `git clone git@github.com:minhnhut-n/device-driver.git`
2. Configure STM32CubeIDE / .ioc as needed.
3. Copy `lib.h` and `lib.c` into your `Core/Inc` directory.

### Configuration Screens (Reference)
These are the settings used in the sample project.

![System clock](../../docs/System%20clock%20configs.png)
![GPIO](../../docs/GPIO_Configuration.png)
![SPI](../../docs/SPI_Configuration.png)
![UART](../../docs/UART_for_serial_debug.png)

## Usage
Use the sample from `rf24_sample` as a reference. Basic flow:
1. Initialize the radio.
2. Select TX or RX mode with the same address/channel on both devices.
3. Send/receive fixed 32-byte payloads.

Minimal example (STM32):
```c
rf24_init();
rf24_tx_mode(address, 12);
rf24_transmit(tx_data);
```

Minimal example (RX):
```c
rf24_init();
rf24_rx_mode(address, 12);
if (is_data_available(1)) {
    rf24_receive(rx_buffer);
}
```

## Notes
- Ensure both sides match: address width, channel, data rate, auto-ack, CRC, and payload size.
- Use a stable 3.3V supply with decoupling near the nRF24 module.
