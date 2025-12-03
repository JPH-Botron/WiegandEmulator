The project uses a stock Raspberry Pi Pico 2 RP2350 board mounted on a custom board with additional hardware and a LCD.

RP2350 pin | Signal        | Notes
-----------|---------------|----------------
GPIO0      | Active_A      | Low true LED that indicates activity on channel A
GPIO2      | DA0_RX        | Wiegand D0 Receive line for channel A
GPIO3      | DA1_RX        | Wiegand D1 Receive line for channel A
GPIO4      | /DA0_TX       | Inverted Wiegand transmit D0 line for channel A
GPIO1      | /DA1_TX       | Inverted Wiegand transmit D1 line for channel A
GPIO5      | Active_B      | Low true LED that indicates activity on channel B
GPIO6      | DB0_RX        | Wiegand D0 Receive line for channel B
GPIO7      | DB1_RX        | Wiegand D1 Receive line for channel B
GPIO9      | /DB0_TX       | Inverted Wiegand transmit D0 line for channel B
GPIO8      | /DB1_TX       | Inverted Wiegand transmit D1 line for channel B
GPIO15     | Active_C      | Low true LED that indicates activity on channel C
GPIO12     | DC0_RX        | Wiegand D0 Receive line for channel C
GPIO13     | DC1_RX        | Wiegand D1 Receive line for channel C
GPIO11     | /DC0_TX       | Inverted Wiegand transmit D0 line for channel C
GPIO14     | /DC1_TX       | Inverted Wiegand transmit D1 line for channel C
GPIO17     | SCL0          | I2C clock for LCD touch controller
GPIO16     | SDA0          | I2C data for LCD touch controller
GPIO18     | SCLK0         | SPI Clock going to LCD
GPIO19     | MOSI0         | SPI MOSI going to LCD
GPIO20     | LCD_DC        | LCD Data / Command line
GPIO21     | LCD_RST       | Reset line for LCD
GPIO22     | LCD_CS        | Chip Select for LCD
GPIO26     | SD_CS         | Chip select for SD Card, also on SPI0
GPIO27     | CTP.INT       | Interrupt line for LCD touch controller
GPIO28     | N/C           | Not used

UART uses the USB serial port on the RP2350 board

The LCD is a 480×320 SPI TFT (ST7796S) with capacitive touch (FT6336U), on SPI0 using the above defined pins.