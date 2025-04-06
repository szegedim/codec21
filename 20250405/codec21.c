// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above
// TODO This file is work in progress.

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <stdio.h>

const size_t linear_length = 20;
const size_t quantized_size = 8;
const size_t lut_size = 30; // Optimal 50
const int linear_tolerance = 6;

// Structure to represent a 3D byte vector, literally a red, green, blue pixel
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t z;
} Vector3D;

// Function to calculate squared distance between two 3D vectors
uint32_t vector_distance_sq(Vector3D a, Vector3D b) {
    int32_t dx = a.x - b.x;
    int32_t dy = a.y - b.y;
    int32_t dz = a.z - b.z;
    return dx*dx + dy*dy + dz*dz;
}

typedef enum {
    VERB_SKIP =    0b000 << 5,  // 0x00
    VERB_LINEAR =  0b001 << 5,  // 0x20
    VERB_LOOKUP =  0b010 << 5,  // 0x40
    VERB_BIT7AND6 = 0b011 << 5, // 0x60
    VERB_BIT5AND4 = 0b100 << 5, // 0x80
    VERB_BIT3AND2 = 0b101 << 5, // 0xA0
    VERB_BIT1AND0 = 0b110 << 5, // 0xC0
} VerbList;

// Masks for extracting verb and length
#define VERB_MASK 0xE0      // Bits 7-5
#define LENGTH_FLAG 0x10    // Bit 4 - indicates if extended length is used
#define SHORT_LENGTH_MASK 0x0F  // Bits 3-0 for short length
#define EXT_LENGTH_BITS 8   // Number of bits in the extension byte
#define MAX_SHORT_LENGTH 15  // Maximum length that can be encoded in 4 bits
#define MAX_BLOCK_LENGTH 4095  // Maximum length that can be encoded in 12 bits (4+8)

// Function to write a block header with length
size_t start_block(uint8_t verb, size_t length, uint8_t* output) {
    size_t bytes_written = 1;
    
    if (length <= MAX_SHORT_LENGTH) {
        // Use short format (4 bits)
        output[0] = verb | (length & SHORT_LENGTH_MASK);
    } else {
        // Use extended format (4+8 = 12 bits)
        output[0] = verb | LENGTH_FLAG | (length & SHORT_LENGTH_MASK);
        output[1] = (length >> 4) & 0xFF;
        bytes_written = 2;
    }
    
    return bytes_written;
}

// Function to read a block header with length
size_t open_block(const uint8_t* input, uint8_t* verb, size_t* length) {
    size_t bytes_read = 1;
    *verb = input[0] & VERB_MASK;
    
    if (input[0] & LENGTH_FLAG) {
        // Extended length format
        *length = (input[0] & SHORT_LENGTH_MASK) | ((size_t)input[1] << 4);
        bytes_read = 2;
    } else {
        // Short length format
        *length = input[0] & SHORT_LENGTH_MASK;
    }
    
    return bytes_read;
}

// Function to find most frequent values in an array for lookup tables
int find_most_frequent(const Vector3D* data, size_t length,
                        Vector3D* lut, size_t lut_size) {
    // Simple frequency counting
    typedef struct {
        Vector3D value;
        int count;
    } FreqEntry;

    FreqEntry freq[length];
    size_t unique_count = 0;

    for (size_t i = 0; i < length; i++) {
        freq[i].count = 0;
    }

    // Count frequencies
    for (size_t i = 0; i < length; i++) {
        bool found = false;
        for (size_t j = 0; j < unique_count; j++) {
            // 20% compression improvement of 4 vs 1
            uint32_t threshold = 8 * 8 * sizeof(Vector3D);
            if (vector_distance_sq(data[i], freq[j].value) < threshold) {
                freq[j].count++;
                found = true;
                break;
            }
        }
        if (!found && unique_count < length) {
            freq[unique_count].value = data[i];
            freq[unique_count].count = 1;
            unique_count++;
        }
    }

    // Sort by frequency
    for (size_t i = 0; i < unique_count - 1; i++) {
        for (size_t j = 0; j < unique_count - i - 1; j++) {
            if (freq[j].count < freq[j + 1].count) {
                FreqEntry temp = freq[j];
                freq[j] = freq[j + 1];
                freq[j + 1] = temp;
            }
        }
    }

    // Copy top N values to LUT
    int count = 0;
    for (size_t i = 0; i < lut_size && i < unique_count; i++) {
        lut[i] = freq[i].value;
        count += freq[i].count;
    }
    return count;
}

// Function to check if points fit a linear line within tolerance
bool is_linear_fit(const Vector3D points[], int count, int tolerance) {
    if (count < 3) return false;
    
    // Check each dimension separately
    for (int dim = 0; dim < 3; dim++) {
        uint8_t* values = (uint8_t*)points;
        double first = values[0 * sizeof(Vector3D) + dim];
        double last = values[(count-1) * sizeof(Vector3D) + dim];
        double slope = (last - first) / (count - 1);
        
        // Check if intermediate points fit the line
        for (int i = 1; i < count-1; i++) {
            double expected = first + slope * i;
            double actual = values[i * sizeof(Vector3D) + dim];
            if (abs(actual - expected) > tolerance){
                return false;
            }
        }
    }
    return true;
}

// Function to check if block has differences that may be solved by a look up table
bool has_lut_differences(const Vector3D* input, const Vector3D* reference, 
                         size_t length) {
    for (size_t i = 0; i < length; i++) {
        int dx = abs((int)(input[i].x) - (int)(reference[i].x));
        int dy = abs((int)(input[i].y) - (int)(reference[i].y));
        int dz = abs((int)(input[i].z) - (int)(reference[i].z));
        if (dx > 32 || dy > 32 || dz > 32) {
            return true;
        }
    }
    return false;
}

// Function to determine difference range for a block
typedef enum {
    DIFF_SMALL,
    DIFF_MEDIUM,
    DIFF_SIGNIFICANT,
    DIFF_LARGE,
    DIFF_REMAINDER
} DiffRange;

DiffRange get_diff_range(const Vector3D* input, const Vector3D* reference, size_t length) {
    bool has_medium = false;
    
    for (size_t i = 0; i < length; i++) {
        int dx = abs((int)(input[i].x) - (int)(reference[i].x));
        int dy = abs((int)(input[i].y) - (int)(reference[i].y));
        int dz = abs((int)(input[i].z) - (int)(reference[i].z));
        
        if (dx >= 16 || dy >= 16 || dz >= 16) {
            return DIFF_LARGE;
        }
        if (dx >= 4 || dy >= 4 || dz >= 4) {
            has_medium = true;
        }
    }
    
    return has_medium ? DIFF_MEDIUM : DIFF_SMALL;
}

// Function to encode LUT blocks similar to the compression ratio to PNG
size_t encode_linear(const Vector3D* input, const Vector3D* reference,
                 size_t input_size, uint8_t* output) {
    size_t output_pos = 0;
    size_t input_pos = 0;
    
    if (input_pos + linear_length < input_size) {
        if (is_linear_fit(&input[input_pos], linear_length, linear_tolerance)) {
            // Write block header with verb and length
            output_pos += start_block(VERB_LINEAR, linear_length, &output[output_pos]);
            
            memcpy(&output[output_pos], &input[input_pos], sizeof(Vector3D));
            output_pos += sizeof(Vector3D);
            memcpy(&output[output_pos], &input[input_pos + linear_length - 1], sizeof(Vector3D));
            output_pos += sizeof(Vector3D);
            input_pos += linear_length;
            return output_pos;
        }
    }

    return 0;
}

// Update encode_lut function
size_t encode_lut(const Vector3D* input, const Vector3D* reference,
                 size_t input_size, uint8_t* output) {
    size_t output_pos = 0;
    size_t input_pos = 0;
    
    // Ensure lut_size fits in our length field
    size_t block_length = (lut_size <= MAX_BLOCK_LENGTH) ? lut_size : MAX_BLOCK_LENGTH;
    
    if (input_pos + block_length <= input_size) {
        // Create LUT with 4 most frequent
        Vector3D lut[4];
        size_t size = find_most_frequent(&input[input_pos], block_length, lut, 4);
        if (size < block_length) {
            return output_pos;
        }

        // Write block header with verb and length
        output_pos += start_block(VERB_LOOKUP, block_length, &output[output_pos]);
        
        memcpy(&output[output_pos], lut, sizeof(Vector3D) * 4);
        output_pos += sizeof(Vector3D) * 4;

        // Encode each value as 2-bit index to nearest LUT entry
        uint8_t index_buffer = 0;
        int bit_pos = 0;

        for (size_t i = 0; i < block_length; i++) {
            // Find closest LUT entry
            uint32_t min_dist = UINT32_MAX;
            uint8_t best_index = 0;
    
            for (uint8_t j = 0; j < 4; j++) {
                uint32_t dist = vector_distance_sq(input[input_pos + i], lut[j]);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_index = j;
                }
            }
    
            // Pack 2-bit indices
            index_buffer |= (best_index << bit_pos);
            bit_pos += 2;
    
            if (bit_pos == 8) {
                output[output_pos++] = index_buffer;
                index_buffer = 0;
                bit_pos = 0;
            }
        }

        // Write final byte if needed
        if (bit_pos > 0) {
            output[output_pos++] = index_buffer;
        }

        input_pos += block_length;
        return output_pos;
    }
    
    return output_pos;
}

// Update encode_quantized function
size_t encode_quantized(const Vector3D* input, const Vector3D* reference,
                       size_t input_size, uint8_t* output) {
    size_t output_pos = 0;
    
    // Check each bit pair position (7-6, 5-4, 3-2, 1-0)
    const uint8_t bit_masks[] = {0xC0, 0x30, 0x0C, 0x03};  // Masks for bit pairs
    const uint8_t bit_shifts[] = {6, 4, 2, 0};             // Shifts for each mask position
    const uint8_t verb_codes[] = {VERB_BIT7AND6, VERB_BIT5AND4, VERB_BIT3AND2, VERB_BIT1AND0};
    
    // Ensure block length fits in our length field
    size_t block_length = (input_size <= MAX_BLOCK_LENGTH) ? input_size : MAX_BLOCK_LENGTH;
    
    // Track if we found any differences at all
    bool any_differences = false;
    
    // Examine each bit pair position
    for (int mask_idx = 0; mask_idx < 4; mask_idx++) {
        uint8_t bit_mask = bit_masks[mask_idx];
        uint8_t bit_shift = bit_shifts[mask_idx];
        
        // Check if there's at least one bit difference at this position
        bool has_difference = false;
        for (size_t i = 0; i < block_length && !has_difference; i++) {
            uint8_t x_diff = (input[i].x & bit_mask) ^ (reference[i].x & bit_mask);
            uint8_t y_diff = (input[i].y & bit_mask) ^ (reference[i].y & bit_mask);
            uint8_t z_diff = (input[i].z & bit_mask) ^ (reference[i].z & bit_mask);
            
            if (x_diff || y_diff || z_diff) {
                has_difference = true;
                any_differences = true;
            }
        }
        
        if (has_difference) {
            // Write block header with verb and length
            output_pos += start_block(verb_codes[mask_idx], block_length, &output[output_pos]);
            
            // Pack the bits into a bit stream
            uint8_t bit_buffer = 0;
            int bit_pos = 0;
            
            for (size_t i = 0; i < block_length; i++) {
                // Extract the bit pairs for each component
                uint8_t x_bits = (input[i].x & bit_mask) >> bit_shift;
                uint8_t y_bits = (input[i].y & bit_mask) >> bit_shift;
                uint8_t z_bits = (input[i].z & bit_mask) >> bit_shift;
                
                // Pack bits into the bit buffer
                bit_buffer |= (x_bits << bit_pos);
                bit_pos += 2;
                if (bit_pos >= 8) {
                    output[output_pos++] = bit_buffer;
                    bit_buffer = 0;
                    bit_pos = 0;
                }
                
                bit_buffer |= (y_bits << bit_pos);
                bit_pos += 2;
                if (bit_pos >= 8) {
                    output[output_pos++] = bit_buffer;
                    bit_buffer = 0;
                    bit_pos = 0;
                }
                
                bit_buffer |= (z_bits << bit_pos);
                bit_pos += 2;
                if (bit_pos >= 8) {
                    output[output_pos++] = bit_buffer;
                    bit_buffer = 0;
                    bit_pos = 0;
                }
            }
            
            // Write any remaining bits in the buffer
            if (bit_pos > 0) {
                output[output_pos++] = bit_buffer;
            }
            
            return output_pos;
        }
    }
    
    // If we get here and didn't find any differences, use a VERB_SKIP
    if (!any_differences) {
        output_pos += start_block(VERB_SKIP, block_length, &output[output_pos]);
        return output_pos;
    }
    
    printf("ERROR: No differences found\n");
    output_pos += start_block(VERB_SKIP, block_length, &output[output_pos]);
    return output_pos;
}

// Function to encode blocks
size_t encode_block(const Vector3D* input, const Vector3D* reference, 
                   size_t input_size, uint8_t* output, size_t output_size) {
    size_t output_pos = 0;
    size_t input_pos = 0;
    size_t max_block_length = 100;
    
    while (input_pos < input_size) {
        if (output_pos + max_block_length >= output_size) {
            printf("Overflow\n");
            break;
        }
        // Skip zero difference blocks like h.264, h.265
        size_t zero_length = 0;
        while (zero_length < MAX_BLOCK_LENGTH && input_pos + zero_length < input_size && 
               memcmp(&input[input_pos + zero_length], 
                      &reference[input_pos + zero_length],
                      sizeof(Vector3D)) == 0) {
            zero_length++;
        }
        
        if (zero_length > 0) {
            // Write block header with verb and length
            output_pos += start_block(VERB_SKIP, zero_length, &output[output_pos]);
            input_pos += zero_length;
            continue;
        }

        // Linear encoding for run-length and slopes like PNG
        size_t linear_encoded = encode_linear(&input[input_pos], &reference[input_pos],
                                        input_size - input_pos, &output[output_pos]);
        if (linear_encoded > 0) {
            output_pos += linear_encoded;
            input_pos += linear_length;
            continue;
        }

        // Lookup table encoding like GIF
        size_t lut_encoded = encode_lut(&input[input_pos], &reference[input_pos],
                                        input_size - input_pos, &output[output_pos]);
        if (lut_encoded > 0) {
            output_pos += lut_encoded;
            input_pos += (lut_size <= MAX_BLOCK_LENGTH) ? lut_size : MAX_BLOCK_LENGTH;
            continue;
        }

        size_t fine_length = input_size - input_pos;
        if (fine_length > quantized_size) {
            fine_length = quantized_size;
        }
        if (fine_length > MAX_BLOCK_LENGTH) {
            fine_length = MAX_BLOCK_LENGTH;
        }

        // Quantized encoding like JPEG-XS
        size_t quantized_encoded = encode_quantized(&input[input_pos], &reference[input_pos],
                                                    fine_length, &output[output_pos]);
        if (quantized_encoded > 0) {
            output_pos += quantized_encoded;
            input_pos += fine_length;
            continue;
        }
    }
    
    return output_pos;
}

// Function to decode blocks using bit masks similar to the encoder
size_t decode_blocks(const uint8_t* input, size_t input_size, 
                    Vector3D* output, const Vector3D* reference) {
    size_t input_pos = 0;
    size_t output_pos = 0;
    
    // Same bit masks and shifts as used in encoding
    const uint8_t bit_masks[] = {0xC0, 0x30, 0x0C, 0x03};  // Masks for bit pairs
    const uint8_t bit_shifts[] = {6, 4, 2, 0};             // Shifts for each mask position
    const uint8_t verb_codes[] = {VERB_BIT7AND6, VERB_BIT5AND4, VERB_BIT3AND2, VERB_BIT1AND0};
    const uint8_t high_bit_masks[] = {0x00, 0xC0, 0xF0, 0xFC}; // Masks for bits more significant than current mask
    
    // Add dithering masks for lower bits with alternating patterns
    const uint8_t dithering_masks_even[] = {
        ~(high_bit_masks[0] | bit_masks[0]) & 0xAA,  // For VERB_BIT7AND6 (even positions)
        ~(high_bit_masks[1] | bit_masks[1]) & 0xAA,  // For VERB_BIT5AND4 (even positions)
        ~(high_bit_masks[2] | bit_masks[2]) & 0xAA,  // For VERB_BIT3AND2 (even positions)
        ~(high_bit_masks[3] | bit_masks[3]) & 0xAA   // For VERB_BIT1AND0 (even positions)
    };
    
    // Second set of dithering masks using 0x55 pattern for odd positions
    const uint8_t dithering_masks_odd[] = {
        ~(high_bit_masks[0] | bit_masks[0]) & 0x55,  // For VERB_BIT7AND6 (odd positions)
        ~(high_bit_masks[1] | bit_masks[1]) & 0x55,  // For VERB_BIT5AND4 (odd positions)
        ~(high_bit_masks[2] | bit_masks[2]) & 0x55,  // For VERB_BIT3AND2 (odd positions)
        ~(high_bit_masks[3] | bit_masks[3]) & 0x55   // For VERB_BIT1AND0 (odd positions)
    };
    
    while (input_pos < input_size) {
        uint8_t block_type;
        size_t length;
        
        // Read block header (verb and length)
        input_pos += open_block(&input[input_pos], &block_type, &length);
        
        switch (block_type) {
            case VERB_SKIP: {  // Skip block
                // Copy directly from reference
                memcpy(&output[output_pos], &reference[output_pos], 
                       length * sizeof(Vector3D));
                output_pos += length;
                break;
            }
            
            case VERB_LINEAR: {  // Linear block
                Vector3D start, end;
                memcpy(&start, &input[input_pos], sizeof(Vector3D));
                input_pos += sizeof(Vector3D);
                memcpy(&end, &input[input_pos], sizeof(Vector3D));
                input_pos += sizeof(Vector3D);
                
                // Interpolate points
                for (int i = 0; i < length; i++) {
                    float t = ((float)i) / (float)(length - 1);
                    output[output_pos + i].x = start.x + t * (end.x - start.x);
                    output[output_pos + i].y = start.y + t * (end.y - start.y);
                    output[output_pos + i].z = start.z + t * (end.z - start.z);
                }
                output_pos += length;
                break;
            }
            
            case VERB_LOOKUP: {  // LUT block
                Vector3D lut[4];
                memcpy(lut, &input[input_pos], sizeof(Vector3D) * 4);
                input_pos += sizeof(Vector3D) * 4;
    
                uint8_t bit_buffer = 0;
                int bits_remaining = 0;
    
                for (size_t i = 0; i < length; i++) {
                    if (bits_remaining < 2) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
        
                    // Extract 2-bit index
                    uint8_t index = bit_buffer & 0x03;
                    bit_buffer >>= 2;
                    bits_remaining -= 2;

                    // Copy vector from LUT
                    output[output_pos++] = lut[index];
                }
                break;
            }
            
            // Handle all bit pair cases with a common approach
            case VERB_BIT7AND6:
            case VERB_BIT5AND4:
            case VERB_BIT3AND2:
            case VERB_BIT1AND0: {
                // Find the corresponding mask and shift
                int mask_idx;
                for (mask_idx = 0; mask_idx < 4; mask_idx++) {
                    if (verb_codes[mask_idx] == block_type) {
                        break;
                    }
                }
                
                uint8_t bit_mask = bit_masks[mask_idx];
                uint8_t bit_shift = bit_shifts[mask_idx];
                uint8_t high_bits_mask = high_bit_masks[mask_idx];
                
                // Process each vector
                uint8_t bit_buffer = 0;
                int bits_remaining = 0;
                
                for (size_t i = 0; i < length; i++) {
                    // Select the appropriate dithering mask based on position
                    uint8_t dithering_mask = (output_pos % 2 == 0) ? 
                        dithering_masks_even[mask_idx] : 
                        dithering_masks_odd[mask_idx];
                    
                    // Process X component
                    if (bits_remaining < 2) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
                    uint8_t x_bits = (bit_buffer & 0x03) << bit_shift;
                    bit_buffer >>= 2;
                    bits_remaining -= 2;
                    
                    // Process Y component
                    if (bits_remaining < 2) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
                    uint8_t y_bits = (bit_buffer & 0x03) << bit_shift;
                    bit_buffer >>= 2;
                    bits_remaining -= 2;
                    
                    // Process Z component
                    if (bits_remaining < 2) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
                    uint8_t z_bits = (bit_buffer & 0x03) << bit_shift;
                    bit_buffer >>= 2;
                    bits_remaining -= 2;
                    
                    // Reconstruct vectors
                    output[output_pos].x = (reference[output_pos].x & high_bits_mask) | x_bits | dithering_mask;
                    output[output_pos].y = (reference[output_pos].y & high_bits_mask) | y_bits | dithering_mask;
                    output[output_pos].z = (reference[output_pos].z & high_bits_mask) | z_bits | dithering_mask;
                    output_pos++;
                }
                break;
            }
        }
    }
    
    return output_pos;
}

