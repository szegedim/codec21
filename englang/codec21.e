#!/bin/englang

Generate c language encoding and decoding code for CODEC21.

This document is Licensed under Creative Commons CC0.
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
to this document to the public domain worldwide.
This document is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this document.
If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

Use an 1024 item vector of three dimensional bytes as the input.
Use the reference vector with the same type as the input vector.

Walk the input vector comparing each item with the corresponding item in the reference vector.
Emit a stream for each block with zero difference in pixels with a 0x00 skip byte, and a block length in 3d items.
Write also a function that identifies and decodes such a block into an array.
Follow the instructions below starting any indexes where the 3d items are different.

Pick 20 items ahead from the first differing item. Use the beginning, end, and three evenly dropped points in the 20 indexes.
Check, whether there they align to a linear line by each dimension with a difference of -2 to zero per dimension.
If there is a 3D linear line that can be alined to all three dimensions with the five point check between the two points, then emit a slice of the linear block with a 0x55 byte, length as 20, and the two pixels at the beginning and the end.
Write also a function that identifies and decodes such a block back ont an array.
Create an empty code function called encode_lut that encodes the remaining slice. 

---

Write the encode_lut function the following way.
Pick the next 25 items starting with the differing item. If there is at least one dimension of each item that differs from the reference more than 16, then encode the entire block of 25 items the following way.
Pick the four most prevalent unique 3d values into a lookup table of four items, and write the block emitting a two bit index for each item representing the index of the closest look up table item.
Emit a unique LUT byte, the length, the lookup table of four 3D items, and the bit vector of two bit indices.
Write also a function that identifies and decodes such a block back onto an array using a block and a reference vector.
Write a stub of function encode_quantized, that handles blocks, if the difference is less, than 16 for all 3D items of the block.
Print the entire file with all encoding and decoding code.

---

Write the encode_quantized function the following way.
If any of the reference items has 16 or more to the input, or the input is less than the reference, then emit the block with a unique 0xEE byte, length, and merge LSB bits 7 to 4 of each dimension into a bit array.
If all the block items have less than 16 but 4 or more difference to the reference item in all 3 dimensions then emit the block with a unique 0xCC byte, length, and merge LSB bits 2 and 3 of each dimension into a bit array.
If all the block items have less than 4 difference compared to the reference item in all 3 dimensions then emit the block with a unique 0xBB byte, length, and merge LSB bits 1 and 0 of each dimension into a bit array.
Create the corresponding decode blocks of setting the upper four bits for each dimension for 0xEE, and adding bits 2,3 for 0xCC, and 1, 0 for 0xBB respectively.

---

Print the entire file with all encoding and decoding code that uses skip, linear encoding, lut, bits 3,2 or bits 1,0 quantization.
