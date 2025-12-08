This document describes the bit ordering used in this tool.

Wiegand data is always transmitted MSB-first: the card sends the first (most significant) bit of the 26-bit frame first, followed by the rest in order, with the final bit of the frame being the least significant bit. A receiver that processes the stream in the canonical way simply shifts its accumulator left for each incoming bit and ORs in the new bit, so the bit that arrived first ends up in the high position of the 26-bit value, and the bit that arrived last becomes the LSB. This produces a buffer in true Wiegand bit order, matching the specification for facility code, ID number, and parity fields.

Where systems differ is in how they display that accumulated value. Some systems directly interpret the 26 collected bits as a binary number and print them as hex, which preserves the logical meaning of the Wiegand frame. Other systems reverse the bit order—treating the first received bit as the LSB—and then place the reversed stream into a 32-bit integer before applying the underlying processor’s byte endianness. This produces a very different hex value even though the underlying Wiegand bits are the same. Thus, Wiegand transmission is fixed and unambiguous (MSB-first), but receivers may represent the captured bits differently depending on how they accumulate, align, and format the bitstream.

We stick with the canonical form.  Here's an example.  

I have a 26 bit card.  I swipe it.  The bits that are read are:

0 0 1 1 0 1 1 0 0 1 0 0 1 1 1 1 1 0 1 1 1 0 0 1 0 1

The first bit transmitted is the zero on the left.  The last bit transmitted is the one on the right.

To display this value as hex, one must divide the buffer into nybbles.  The rightmost 4 bits will be the low order nybble, the next 4 bits from the right will be the next nybble, and so on.

Let's separate the above bit stream this way:

 0 0    1 1 0 1   1 0 0 1   0 0 1 1   1 1 1 0   1 1 1 0   0 1 0 1
0        D         9         3         E         E         5

This is the way most things do it, and it makes the msot sense to read.  The only (minor) issue
is that the first nybble printed is a partial nybble and has to be found by using the received
number of bits.  This will yield a partial nybble at the most significant end, and that nybble 
must have the 'extra' bits on the left set to 0.  In practice, a message is received by shifting 
into a big shift buffer, and the buffer is set to zero before a message is received.

As mentioned above, it is NOT the only way in the universe that anyone does this.  It is, however, 
the most common method, and it is the method used in the Elite V2's decoder as well as the method
used in this tool.