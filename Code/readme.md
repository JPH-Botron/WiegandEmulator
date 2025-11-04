We need a simple PCB that will take serial commands and send or receive Wiegand data.  Wiegand data will 
consist of a length in bits and a hex string with the actual ones and zeros.  We'll do this with
a raspberry pi pico board with code in C.  We'll use the USB port on the Pico board for the serial
side, and we will have two wiegand 'ports' consisting of a D0 and D1 for each port. Each port can
be a transmitter or reciever.  Transmit lines from the Pi Pico will have transistors to form open
collector outputs; receive lines will connect directly to the D0, D1 lines and go into the Pico.  We will
need to be able to set the bit time and the interbit time independently on both transmit ports.
When receiving, we will need to measure the average bit and interbit times found in the message.
Serial commands and messages will be json blobs.