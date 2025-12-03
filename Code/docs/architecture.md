This project does not use malloc.   The three Wiegand channels share as much code as possible, and the majority of the Wiegand receive and transmit operations are handled using PIO timers and are handled
in clearly marked interrupt routines.

The serial port accepts commands and sends responses in a co-operative loop.  The LCD is also handled in the co-operative loop.
