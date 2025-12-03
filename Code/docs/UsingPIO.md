This was my first attempt at using the PIOs, (programmable I/O state machines), so I am including some
info here on how that went.  I used Codex with ChatGPT-5.1-Codex-Max; It was very helpful but could not 
create a working program on its own.  In the end, I still had to fully understand the PIO assembly instructions,
the setup process for SMs, the ways GPIOs are specified, etc.  

The general process is:
o  One writes a PIO program in PIO assembly code.
    o The code does not specify which state amchine to use; that's specified when your mainline C code sets it upt
    o The code does not specify which input or output pins to use; that is specified in the mainline setup.
    o The end result is that if you have N of the same PIO state machines running for your project, they
      all use the same code, and the specific pins are specified when the SMs are 'loaded' by the mainline code.

o  One uses the PIO assembler (outside the normal build process, this is a manual step)
    o There is a different assembler for each of the RP2040 and the RP2530
    o The pio assembler is installed as part of platformio, but it is not in the path.  You have to go find it.

o  The PIO assembler generates a ".h" file for use with C code.  As above, this assembled code does not
   specify which pins or which specific state machine will be used.

o  The main C code makes calls to grab a state machine, initialize which pins it will use, and set some options about
   how that SM will operate (e.g. the input and output FIFOs, shift directions, etc).
    o  This is *important*.  The full setup is not always given in examples, as the defaults are often acceptable,
       but 'often' is not 'always'.  I had to fight this problem where an invisible default (nothing in the example code that even mentioned it) was causing me problems.

o  The main C code loads the assembled PIO instructions from the .h file (output of the assembler) into the 
selected SM.  

o  The SM starts running the code (not sure exactly what triggers it to start)




Here is my full command for compiling the pio porgram and saving the resulting .h file:

\Users\paul.BDC\.platformio\packages\tool-pioasm-rp2040-earlephilhower\pioasm -o c-sdk wietest\src\wiegand_rx2.pio wietest\src\wiegand_rx2pio.h
