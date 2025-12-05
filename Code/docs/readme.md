This project uses an RP2350 on a custom board to implement 3 channels of a Wiegand transmit/receive test system.  
Note that revision A of the PCB has a number of errors and requires a bunch of cut and jumps.  

Each Wiegand channel is independent and can be used to transmit or receive Wiegand data.  There is an activity LED on each channel that momentarily lights if a message is transmitted or received.  In addition, there is an LCD touch screen that shows what is being transmitted or received on each channel.  The board runs fine if the LCD display is not installed.

When a Wiegand message is received on a channel, we record the minimum, maximum and average bit time on both D0 and D1 with at least 1 microsecond precision.  We also record the minimum, maximum and average inter-bit time, and finally, we record the actual message received and its length (in bits).  T

THe serial port can also command that a message be sent on any channel.  The command includes the message, the message length, the bit time (symmetric to D0 and D1) and the inter-bit time.

When a channel is transmitting, the receive is also active for that channel and will receive the transmitted data.

The board has a touch LCD that is used as a scrolling terminal, showing each message transmitted or received.

General usage:  Connect this board to a PC via the USB cable (which should look like a serial port)
connect the Wiegand port to whatever thing you like.  Each port is bidirection, but you may (probably will) need to
enable the pullups (via jumper on the PCB).  

You can now send out ay of the three Wiegand ports (a, b or c) by using the tx command:

tx a 1122334455 26 85 36

The above command sends the message (starting with bit 0 of the leftmost byte) out the Wiegand A port.
It sends 26 bits, where the bit time is 85 uS and the interbit time is 36 uSec.  It's up to you to wait and
send the next message; that sets the inter-message delay time.

When you send that message, the reciever on the same port will hear it and buffer the message it heard.  You can get all the messages the board has heard by using the getrx command.  It sends a json list of all the messages it has
seen.  If I have only sent the above message, the json response to getrx would be:

[{"port":"a","bits":26,"pulse":[85,85,98],"gap":[36,36,44],"data":"0x11223300"}]

The three points for pulse and gap are min, average, and max time for the associated value.

One of the main points for creating this board is to test the wiegand ports on the V3 and the V2.

A help command exists that shows all the available commands; the others are of occasional use:
help
Commands:
  ping
  ver
  help
  pins <a|b|c>
  getrx
  tx <a|b|c> <hexdata> [bits] [bit_us] [inter_us]

The 'pins' command exists to help determine if the port has pullups installed at all.  It simply tells you the
current state of D0 and D1 on a given port.

pins a
{"port":"a","d0":1,"d1":1}


Toolchain: “VS Code + PlatformIO + Codex, framework = Arduino-Pico on RP2350”.

How to:

    Build: pio run
    The actual pio hardware IO state machince code must be compiled from wiegand_rx.pio to wiegand_rx.h external to vscode; a project build will not rebuild the HW assembly code.

    Upload: pio run -t upload or “hold BOOTSEL and plug USB, then drag UF2”

    Open serial monitor: pio device monitor

Schematic and PCB layout are in Kicad; project is called WiegandTest



