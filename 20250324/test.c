#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <stdio.h>
#include "codec21.h"

// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

/*
gcc -o a.out test.c codec21.c; ./a.out 
*/

void calculate_errors(const size_t num, Vector3D* input, Vector3D* decompressed) {
    // Calculate mean quadratic difference between input and decompressed data
    double sum_diff_x = 0.0, sum_diff_y = 0.0, sum_diff_z = 0.0;

    for (size_t i = 0; i < num; i++) {
        // Calculate squared differences for each dimension
        double diff_x = (double)input[i].x - (double)decompressed[i].x;
        double diff_y = (double)input[i].y - (double)decompressed[i].y;
        double diff_z = (double)input[i].z - (double)decompressed[i].z;
    
        double squared_diff_x = diff_x * diff_x;
        double squared_diff_y = diff_y * diff_y;
        double squared_diff_z = diff_z * diff_z;

        sum_diff_x += squared_diff_x;
        sum_diff_y += squared_diff_y;
        sum_diff_z += squared_diff_z;
    }

    // Calculate averages
    double avg_diff_x = sum_diff_x / num;
    double avg_diff_y = sum_diff_y / num;
    double avg_diff_z = sum_diff_z / num;
    // Print results
    printf("Average quadratic differences:\n");
    printf("X dimension: %.6f\n", avg_diff_x);
    printf("Y dimension: %.6f\n", avg_diff_y);
    printf("Z dimension: %.6f\n", avg_diff_z);
}

void clear(Vector3D* buffer, size_t num_vectors) {
    for (size_t i = 0; i < num_vectors; i++) {
        buffer[i].x = 0;
        buffer[i].y = 0;
        buffer[i].z = 0;
    }
}

void unit_test_0(Vector3D* buffer, size_t num_vectors) {
}

void unit_test_1(Vector3D* buffer, size_t num_vectors) {
    const uint8_t start_value = 0x10;
    const uint8_t end_value = 0xA0;
    
    // Calculate step size for smooth gradient
    float step = (float)(end_value - start_value) / (num_vectors - 1);
    
    for (size_t i = 0; i < num_vectors; i++) {
        // Calculate current gradient value
        uint8_t value = (uint8_t)(start_value + (step * i));
        
        // Set all RGB components to same value for grayscale
        buffer[i].x = value;
        buffer[i].y = value;
        buffer[i].z = value;
    }
}

void unit_test_2(Vector3D* buffer, size_t num_vectors) {
    size_t current_pos = 0;
    bool is_white = true;  // Start with white
    
    while (current_pos < num_vectors) {
        // Generate random length between 5 and 100
        size_t slice_length = 5 + (rand() % 120);  // 5 to 100 inclusive
        
        // Make sure we don't exceed buffer size
        if (current_pos + slice_length > num_vectors) {
            slice_length = num_vectors - current_pos;
        }
        
        // Fill the slice with either white (0xFF) or black (0x00)
        uint8_t value = is_white ? 0xFF : 0x00;
        
        for (size_t i = 0; i < slice_length; i++) {
            buffer[current_pos + i].x = value;
            buffer[current_pos + i].y = value;
            buffer[current_pos + i].z = value;
        }
        
        // Move position and alternate color
        current_pos += slice_length;
        is_white = !is_white;
    }
}

void unit_test_3(Vector3D* buffer, size_t num_vectors) {
    // Fill the entire buffer with grey (brightness 63)
    for (size_t i = 0; i < num_vectors; i++) {
        buffer[i].x = 0x3F;
        buffer[i].y = 0x3D;
        buffer[i].z = 0x3E;
    }
}

void unit_test_4(Vector3D* buffer, size_t num_vectors) {
    // Fill the entire buffer with grey (brightness 63)
    for (size_t i = 0; i < num_vectors; i++) {
        buffer[i].x = 0x3F;
        buffer[i].y = 0x3D;
        buffer[i].z = 0x4;
    }
}

void unit_test_5(Vector3D* buffer, size_t num_vectors) {
    // Fill the buffer with random values
    for (size_t i = 0; i < num_vectors; i++) {
        buffer[i].x = rand() & 0xFF; // Random value 0-255
        buffer[i].y = rand() & 0xFF; // Random value 0-255
        buffer[i].z = rand() & 0xFF; // Random value 0-255
    }
}

int tests() {
    // Sample size for testing
    const size_t NUM_VECTORS = 1024;
    
    // Allocate input, reference, and output buffers
    Vector3D* input = malloc(NUM_VECTORS * sizeof(Vector3D));
    Vector3D* reference = malloc(NUM_VECTORS * sizeof(Vector3D));
    Vector3D* decompressed = malloc(NUM_VECTORS * sizeof(Vector3D));
    
    // Maximum compressed size (worst case)
    const size_t uncompressed_size = NUM_VECTORS * sizeof(Vector3D) * 1;
    size_t max_compressed_size = NUM_VECTORS * sizeof(Vector3D) * 2;
    uint8_t* compressed = malloc(max_compressed_size);
    
    void (*tests[])(Vector3D* buffer, size_t num_vectors) = {
        unit_test_0,
        unit_test_1,
        unit_test_2,
        unit_test_3,
        unit_test_4,
        unit_test_5
    };
     
    for (size_t t=0; t<sizeof(tests) / sizeof(tests[0]); t++) {
        clear(reference, NUM_VECTORS);
        clear(decompressed, NUM_VECTORS);
        tests[t](input, NUM_VECTORS);
        for (size_t i=0; i < 6; i++) {

            size_t compressed_size = 0;
            compressed_size += encode_block(input, reference, NUM_VECTORS, compressed + compressed_size, max_compressed_size - compressed_size);
        
            size_t decompressed_vectors = decode_blocks(compressed, compressed_size, 
                                                      decompressed, reference);
    /*
            for (size_t j=100; j < 350; j++) {
                printf("GG %2x %2x %2x, ", input[j].x, input[j].y, input[j].z);
            }
            printf("\n");
            for (size_t j=100; j < 350; j++) {
                printf("HH %2x %2x %2x, ", reference[j].x, reference[j].y, reference[j].z);
            }
            printf("\n");
            for (size_t j=100; j < 350; j++) {
                printf("JJ %2x %2x %2x, ", decompressed[j].x, decompressed[j].y, decompressed[j].z);
            }
            printf("\n");
    */

            memcpy(reference, decompressed, uncompressed_size);

            printf("\nProgressive frame: %d\nCompression ratio: %.6f/%.6f = %.6f%%\n",
                   i,
                   (double)compressed_size, (double)uncompressed_size, 
                   100.0*((double)compressed_size) / (double)uncompressed_size);
    
            calculate_errors(NUM_VECTORS, input, decompressed);
        } 
    }


    free(input);
    free(reference);
    free(decompressed);
    free(compressed);
    
    return 0;
}

int main(int argc, char *argv[]) {
    tests();    
    return 0;
}
