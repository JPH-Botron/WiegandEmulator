Well, I made several mistakes in Revision A, mainly by not fully understanding that pins
really are not all that interchangable.

1.  The LCD SPI port needs to use specific dedicated pins.  The originals cannot be used.
	o  Cut wires connected to GPIO18, 19, 26, 27, 28
	o  Connect GPIO18 to SCLK0 (J4.8)
	o  Connect GPIO19 to MOSI  (J4.9)
	o  Connect GPIO26 to SD_CS  (J4.1)
	o  Connect GPIO27 to CTP.INT (J4.2)
	o  Leave GPIO28 unconnected.  (MISO not used)
	
2.  Pins for RXD0 and RXD1 must be adjacent, and D0 must be a lower GPIO number than D1.
This is due to the way the PIO state machines work.
    o  For Wiegand A:  Cut wires on GPIO2 and GPIO3
	o  Connect GPIO2 to DA0_RX
	o  Connect GPIO3 to DA1_RX
	
	o  For Wiegand B:  Cut wires on GPIO7 and GPIO8
	o  Connect GPIO7 to DB1_RX
	o  Connect GPIO8 to /DB1_TX
	
	o  Wiegand C is OK as is.
	
	
