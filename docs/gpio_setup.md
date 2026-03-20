# ESP32 Hardware Bridge Driver

This kernel module bridges real GPIO and I2C hardware into the Flipper One QEMU emulator via an ESP32 microcontroller.

## How it works

Flipper One VM (/dev/gpiochip0) → Kernel driver → /dev/ttyUSB0 → ESP32 → Real hardware


The driver registers a native Linux GPIO chip inside the VM. Standard tools like `gpioset` and `gpioget` work without any modifications.

## Protocol

Communication between the driver and ESP32 uses a custom binary protocol over UART at 115200 baud.

### Packet format

| Byte | Field  | Description                    |
|------|--------|--------------------------------|
| 0    | SYNC   | Always 0xAA                    |
| 1    | CMD    | Command byte                   |
| 2    | LEN    | Length of data payload         |
| 3..N | DATA   | Payload bytes                  |
| N+1  | CRC8   | CRC8 over all previous bytes   |

### Response format

| Byte | Field  | Description                    |
|------|--------|--------------------------------|
| 0    | SYNC   | Always 0xAA                    |
| 1    | STATUS | 0x00 = OK, 0xFF = ERROR        |
| 2    | LEN    | Length of response data        |
| 3..N | DATA   | Response bytes                 |
| N+1  | CRC8   | CRC8 over all previous bytes   |

### Commands

| CMD  | Name        | Data            | Response     |
|------|-------------|-----------------|--------------|
| 0x01 | I2C_READ    | addr, nbytes    | read bytes   |
| 0x02 | I2C_WRITE   | addr, bytes...  | -            |
| 0x03 | GPIO_SET    | pin, value      | -            |
| 0x04 | GPIO_GET    | pin             | value        |
| 0x06 | PING        | -               | 0x01         |

### Error codes

| Code | Description   |
|------|---------------|
| 0x01 | Timeout       |
| 0x02 | I2C NACK      |
| 0x03 | Bad CRC       |
| 0x04 | Unknown cmd   |
| 0x05 | Bad length    |

## ESP32 firmware

Flash the file called mcu_protocol.bin on a ESP32

Requirements:

- Any ESP32 board with CH340 USB chip

Tested on ESP32 WROOM DevModule.

## Loading the driver

Inside the Flipper One VM:

```bash
insmod esp32_bridge.ko
```

Check it loaded:

```bash
dmesg | grep ESP32
ls /dev/gpiochip*
insmod esp32_bridge.ko
```


## Usage

Control GPIO from inside the VM:

```bash
# Set pin 2 high
gpioset --chip /dev/gpiochip0 2=1

# Set pin 2 low  
gpioset --chip /dev/gpiochip0 2=0

# Read pin state
gpioget --chip /dev/gpiochip0 2
```


## Current state

- GPIO read/write works
- I2C support coming soon
- SPI support coming soon

## Hardware setup

Connect ESP32 to your PC via USB. The driver uses `/dev/ttyUSB0` by default.

Pass the USB device into the VM via QEMU:

```bash
-device usb-host,hostbus=X,hostaddr=Y
```

Or pass ttyUSB0 directly:

```bash
-serial /dev/ttyUSB0
```


## Disclaimer

This project is not affiliated with Flipper Devices and exists only for experimentation.
