This project uses an RP2350 on a custom board to implement 3 channels of a Wiegand transmit/receive test system.  Each channel is independent and can be used to transmit or receive Wiegand data.  In addition, there is an LCD touch screen
that shows what is being transmitted or received on each channel.

When a Wiegand message is received on a channel, we record the minimum, maximum and average bit time on both D0 and D1 with at least 1 microsecond precision.  We also record the minimum, maximum and average inter-bit time, and finally, we record the actual message received and it's length (in bits).  This data is to be sent upon request to the serial port and back to a PC.

THe serial port can also command that a message be sent on any channel.  The command includes the message, the message length, the bit time (symmetric to D0 and D1) and the inter-bit time.

When a channel is transmitting, the receive is also active for that channel and will receive the transmitted data.

The board has a touch LCD that is used as a scrolling terminal, showing each message transmitted or received.

Toolchain: “VS Code + PlatformIO + Codex, framework = Arduino-Pico on RP2350”.

How to:

    Build: pio run
    The actual pio hardware IO state machince code must be compiled from wiegand_rx.pio to wiegand_rx.h external to vscode; a project build will not rebuild the HW assembly code.

    Upload: pio run -t upload or “hold BOOTSEL and plug USB, then drag UF2”

    Open serial monitor: pio device monitor

Schematic and PCB layout are in Kicad; project is called WiegandTest



