// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t z;
} Vector3D;

extern size_t decode_blocks(const uint8_t* input, size_t input_size, 
                    Vector3D* output, const Vector3D* reference);
size_t encode_block(const Vector3D* input, const Vector3D* reference, 
    size_t input_size, uint8_t* output, size_t output_size);

#define WIDTH 1920
#define HEIGHT 1080
