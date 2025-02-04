#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <stdio.h>

// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// TODO This file is work in progress.

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

// Function to find most frequent values in an array
int find_most_frequent(const Vector3D* data, size_t length,
                        Vector3D* lut, size_t lut_size) {
    // Simple frequency counting
    typedef struct {
        Vector3D value;
        int count;
    } FreqEntry;

    // Allocate memory on the stack
    FreqEntry freq[length];
    size_t unique_count = 0;

    // Initialize the frequency array
    for (size_t i = 0; i < length; i++) {
        freq[i].count = 0;
    }

    // Count frequencies
    for (size_t i = 0; i < length; i++) {
        bool found = false;
        for (size_t j = 0; j < unique_count; j++) {
            if (memcmp(&data[i], &freq[j].value, sizeof(Vector3D)) == 0) {
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
bool is_linear_fit(Vector3D points[], int count, int tolerance) {
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
            if (abs(actual - expected) > tolerance) return false;
        }
    }
    return true;
}

// Function to check if block has large differences
bool has_lut_differences(const Vector3D* input, const Vector3D* reference, 
                         size_t length) {
    for (size_t i = 0; i < length; i++) {
        int dx = abs(input[i].x - reference[i].x);
        int dy = abs(input[i].y - reference[i].y);
        int dz = abs(input[i].z - reference[i].z);
        if (dx > 32 || dy > 32 || dz > 32) {
            return true;
        }
    }
    return false;
}

// Function to check if block has large differences
bool has_large_differences(const Vector3D* input, const Vector3D* reference, 
                         size_t length) {
    for (size_t i = 0; i < length; i++) {
        int dx = abs(input[i].x - reference[i].x);
        int dy = abs(input[i].y - reference[i].y);
        int dz = abs(input[i].z - reference[i].z);
        if (dx > 16 || dy > 16 || dz > 16) {
            return true;
        }
    }
    return false;
}

// Function to determine difference range for a block
typedef enum {
    DIFF_SMALL,  // < 4
    DIFF_MEDIUM, // 4-15
    DIFF_LARGE   // >= 16 or negative
} DiffRange;

DiffRange get_diff_range(const Vector3D* input, const Vector3D* reference, size_t length) {
    bool has_medium = false;
    
    for (size_t i = 0; i < length; i++) {
        int dx = (input[i].x - reference[i].x);
        int dy = (input[i].y - reference[i].y);
        int dz = (input[i].z - reference[i].z);
        
        if (dx >= 16 || dy >= 16 || dz >= 16 || dx < 0 || dy < 0 || dz < 0) {
            return DIFF_LARGE;
        }
        if (dx >= 4 || dy >= 4 || dz >= 4  || dx < 0 || dy < 0 || dz < 0) {
            has_medium = true;
        }
    }
    
    return has_medium ? DIFF_MEDIUM : DIFF_SMALL;
}

DiffRange get_diff_masked(const Vector3D* input, const Vector3D* reference, size_t length) {
    bool has_medium = false;
    
    for (size_t i = 0; i < length; i++) {
        int dx = (input[i].x ^ reference[i].x);
        int dy = (input[i].y ^ reference[i].y);
        int dz = (input[i].z ^ reference[i].z);
        
        if ((dx & 0xf0) | (dy & 0xf0) | (dz & 0xf0)) {
            return DIFF_LARGE;
        }
        if ((dx & 0xfc) | (dy & 0xfc) | (dz & 0xfc)) {
            has_medium = true;
        }
    }
    return has_medium ? DIFF_MEDIUM : DIFF_SMALL;
}

const size_t linear_length = 20;
// Function to encode LUT blocks similar to the compression ratio to PNG
size_t encode_linear(const Vector3D* input, const Vector3D* reference,
                 size_t input_size, uint8_t* output) {
    size_t output_pos = 0;
    size_t input_pos = 0;
    
    if (input_pos + linear_length < input_size) {
        Vector3D check_points[5];
        check_points[0] = input[input_pos];
        check_points[4] = input[input_pos + linear_length];
        check_points[1] = input[input_pos + linear_length / 4];
        check_points[2] = input[input_pos + linear_length / 2];
        check_points[3] = input[input_pos + linear_length * 3 / 4];
            
        DiffRange range = get_diff_range(&input[input_pos], 
                            &reference[input_pos], linear_length);


        if (range == DIFF_LARGE && is_linear_fit(check_points, 5, 2)) {
            output[output_pos++] = 0x55;  // Linear block marker
            const int length = linear_length;
            output[output_pos++] = length;    // Fixed length
            memcpy(&output[output_pos], &input[input_pos], sizeof(Vector3D));
            output_pos += sizeof(Vector3D);
            memcpy(&output[output_pos], &input[input_pos + length - 1], sizeof(Vector3D));
            output_pos += sizeof(Vector3D);
            input_pos += length;
            return output_pos;
        }
    }

    return 0;
}

// Function to encode LUT blocks similar to the compression ratio to GIF
size_t encode_lut(const Vector3D* input, const Vector3D* reference,
                 size_t input_size, uint8_t* output) {
    size_t output_pos = 0;
    size_t input_pos = 0;
    size_t lut_size = 25;
    
    if (input_pos + lut_size <= input_size) {
        if (has_lut_differences(&input[input_pos], &reference[input_pos], lut_size)) {
            // Create LUT with 4 most frequent values
            Vector3D lut[4];
            if (find_most_frequent(&input[input_pos], lut_size, lut, 4) != lut_size) {
                return output_pos;
            }

            output[output_pos++] = 0xAA;  // LUT block marker
            output[output_pos++] = lut_size;    // Block length

            memcpy(&output[output_pos], lut, sizeof(Vector3D) * 4);
            output_pos += sizeof(Vector3D) * 4;
    
            // Encode each value as 2-bit index to nearest LUT entry
            uint8_t index_buffer = 0;
            int bit_pos = 0;
    
            for (size_t i = 0; i < lut_size; i++) {
                // Find closest LUT entry
                uint32_t min_dist = UINT32_MAX;
                uint8_t best_index = 0;
        
                for (uint8_t j = 0; j < 4; j++) {  // Changed from 16 to 4
                    uint32_t dist = vector_distance_sq(input[input_pos + i], lut[j]);
                    if (dist < min_dist) {
                        min_dist = dist;
                        best_index = j;
                    }
                }
        
                // Pack 2-bit indices (changed from 4-bit)
                index_buffer |= (best_index << bit_pos);
                bit_pos += 2;  // Changed from 4 to 2
        
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
    
            input_pos += lut_size;
        }
    }
    
    return output_pos;
}

// Function to encode quantized blocks like H.264/JPEG
size_t encode_quantized(const Vector3D* input, const Vector3D* reference,
                       size_t input_size, uint8_t* output) {
    size_t output_pos = 0;
    size_t input_pos = 0;
    
    size_t fine_length = input_size - input_pos;
    if (fine_length > 25) {
        fine_length = 25;
    }

    if (input_pos < input_size) {
        DiffRange range = get_diff_masked(&input[input_pos], 
                                       &reference[input_pos], fine_length);
        
        if (range == DIFF_MEDIUM) {

            // Encode blocks with differences 4-15 (2-bit encoding)
            output[output_pos++] = 0xCC;  // BIT2 marker
            output[output_pos++] = fine_length;    // Block length

            uint8_t bit_buffer = 0;
            int bit_pos = 0;
            
            for (size_t i = 0; i < fine_length; i++) {

                // Pack bits 2 and 3 of each dimension
                uint8_t x_bits = (input[input_pos + i].x >> 2) & 0x03;
                uint8_t y_bits = (input[input_pos + i].y >> 2) & 0x03;
                uint8_t z_bits = (input[input_pos + i].z >> 2) & 0x03;

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
            
            if (bit_pos > 0) {
                output[output_pos++] = bit_buffer;
            }
            
            input_pos += fine_length;
            return output_pos;
        }
        else if (range == DIFF_SMALL) {
            // Encode blocks with differences < 4 (1-bit encoding)
            output[output_pos++] = 0xBB;  // BIT1 marker
            output[output_pos++] = fine_length;    // Block length
            
            uint8_t bit_buffer = 0;
            int bit_pos = 0;
            
            for (size_t i = 0; i < fine_length; i++) {
                
                // Pack bits 0 and 1 of each dimension
                uint8_t x_bits = input[input_pos + i].x & 0x03;
                uint8_t y_bits = input[input_pos + i].y & 0x03;
                uint8_t z_bits = input[input_pos + i].z & 0x03;
                
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
            
            if (bit_pos > 0) {
                output[output_pos++] = bit_buffer;
            }
            
            input_pos += fine_length;
            return output_pos;
        }
    }
    
    // If we're not returning after processing the block
    output[output_pos++] = 0xEE;  // New marker for remaining data
    output[output_pos++] = fine_length;    // Block length
    
    for (size_t i = 0; i < fine_length; i += 2) {

        uint8_t x1 = (input[input_pos + i].x >> 4) & 0x0F;
        uint8_t y1 = (input[input_pos + i].y >> 4) & 0x0F;
        uint8_t z1 = (input[input_pos + i].z >> 4) & 0x0F;

        if (i + 1 < fine_length) {
            uint8_t x2 = (input[input_pos + i + 1].x >> 4) & 0x0F;
            uint8_t y2 = (input[input_pos + i + 1].y >> 4) & 0x0F;
            uint8_t z2 = (input[input_pos + i + 1].z >> 4) & 0x0F;

            output[output_pos++] = (x1 << 4) | x2;
            output[output_pos++] = (y1 << 4) | y2;
            output[output_pos++] = (z1 << 4) | z2;
        } else {
            output[output_pos++] = (x1 << 4);
            output[output_pos++] = (y1 << 4);
            output[output_pos++] = (z1 << 4);
        }
    }
    
    return output_pos;
}

// Function to encode zero blocks and linear blocks
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
        // Check for zero difference block
        size_t zero_length = 0;
        while (zero_length < 200 && input_pos + zero_length < input_size && 
               memcmp(&input[input_pos + zero_length], 
                      &reference[input_pos + zero_length],
                      sizeof(Vector3D)) == 0) {
            zero_length++;
        }
        
        if (zero_length > 0) {
            output[output_pos++] = 0x00;  // Skip block marker
            output[output_pos++] = zero_length;
            input_pos += zero_length;
            continue;
        }

        // Try linear encoding for run-length and slopes.
        size_t linear_encoded = encode_linear(&input[input_pos], &reference[input_pos],
                                        input_size - input_pos, &output[output_pos]);
        if (linear_encoded > 0) {
            output_pos += linear_encoded;
            input_pos += linear_length;  // LUT uses 25-vector blocks
            continue;
        }

        // Try LUT encoding
        size_t lut_encoded = encode_lut(&input[input_pos], &reference[input_pos],
                                        input_size - input_pos, &output[output_pos]);
        if (lut_encoded > 0) {
            output_pos += lut_encoded;
            input_pos += 25;  // LUT uses 25-vector blocks
            continue;
        }

        // If all failed, try quantized encoding
        size_t quantized_encoded = encode_quantized(&input[input_pos], &reference[input_pos],
                                                    input_size - input_pos, &output[output_pos]);
        if (quantized_encoded > 0) {
            output_pos += quantized_encoded;
            input_pos += 25;  // Quantized also uses 25-vector blocks
            continue;
        } else {
            break;
        }
    }
    
    return output_pos;
}

// Main decoding function that handles all block types
size_t decode_blocks(const uint8_t* input, size_t input_size, 
                    Vector3D* output, const Vector3D* reference) {
    size_t input_pos = 0;
    size_t output_pos = 0;
    
    while (input_pos < input_size) {
        uint8_t block_type = input[input_pos++];
        uint8_t length = input[input_pos++];
        //printf("%x\n", block_type);
        
        switch (block_type) {
            case 0x00: {  // Skip block
                // Copy directly from reference
                memcpy(&output[output_pos], &reference[output_pos], 
                       length * sizeof(Vector3D));
                output_pos += length;
                break;
            }
            
            case 0x55: {  // Linear block
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
            case 0xAA: {  // LUT block
                Vector3D lut[4];
                memcpy(lut, &input[input_pos], sizeof(Vector3D) * 4);
                input_pos += sizeof(Vector3D) * 4;
    
                uint8_t bit_buffer = 0;
                int bits_remaining = 0;
    
                for (size_t i = 0; i < length; i++) {
                    if (bits_remaining < 2) {  // Changed from 4 to 2
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
        
                    // Extract 2-bit index (changed from 4-bit)
                    uint8_t index = bit_buffer & 0x03;  // Changed from 0x0F
                    bit_buffer >>= 2;  // Changed from 4
                    bits_remaining -= 2;  // Changed from 4
        
                    // Copy vector from LUT
                    output[output_pos++] = lut[index];
                }
                break;
            }

            case 0xEE: {  // Additional bits 7-4 block
                size_t block_length = length;
    
                for (size_t i = 0; i < block_length; i += 2) {
                    uint8_t x_packed = input[input_pos++];
                    uint8_t y_packed = input[input_pos++];
                    uint8_t z_packed = input[input_pos++];
        
                    uint8_t x1_upper = (x_packed >> 4) & 0x0F;
                    uint8_t y1_upper = (y_packed >> 4) & 0x0F;
                    uint8_t z1_upper = (z_packed >> 4) & 0x0F;
        
                    output[output_pos].x = (x1_upper << 4);
                    output[output_pos].y = (y1_upper << 4);
                    output[output_pos].z = (z1_upper << 4);
                    output_pos++;
        
                    if (i + 1 < block_length) {
                        uint8_t x2_upper = x_packed & 0x0F;
                        uint8_t y2_upper = y_packed & 0x0F;
                        uint8_t z2_upper = z_packed & 0x0F;
            
                        output[output_pos].x = (x2_upper << 4);
                        output[output_pos].y = (y2_upper << 4);
                        output[output_pos].z = (z2_upper << 4);
                        output_pos++;
                    }
                }
                break;
            }

            case 0xCC: {  // Additional bits 3-2 block (differences 4-15)
                uint8_t bit_buffer = 0;
                int bits_remaining = 0;
                
                for (size_t i = 0; i < length; i++) {                    
                    // Extract 2 bits for each dimension and shift left by 2
                    if (bits_remaining == 0) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
                    uint8_t x_bits = (bit_buffer & 0x03) << 2;
                    bit_buffer >>= 2;
                    bits_remaining -= 2;
                    if (bits_remaining == 0) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
                    uint8_t y_bits = (bit_buffer & 0x03) << 2;
                    bit_buffer >>= 2;
                    bits_remaining -= 2;
                    if (bits_remaining == 0) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
                    uint8_t z_bits = (bit_buffer & 0x03) << 2;
                    bit_buffer >>= 2;
                    bits_remaining -= 2;
                    
                    // Reconstruct vector from reference and difference
                    output[output_pos].x = (reference[output_pos].x & 0xf0) + x_bits;
                    output[output_pos].y = (reference[output_pos].y & 0xf0) + y_bits;
                    output[output_pos].z = (reference[output_pos].z & 0xf0) + z_bits;
                    output_pos++;
                }
                break;
            }
            
            case 0xBB: {  // Additional bits 1-0 block (differences < 4)
                uint8_t bit_buffer = 0;
                int bits_remaining = 0;
                
                for (size_t i = 0; i < length; i++) {
                    // Extract 2 bits for each dimension (using bits 1,0)
                    if (bits_remaining == 0) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
                    uint8_t x_bits = bit_buffer & 0x03;
                    bit_buffer >>= 2;
                    bits_remaining -= 2;
                    if (bits_remaining == 0) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
                    uint8_t y_bits = bit_buffer & 0x03;
                    bit_buffer >>= 2;
                    bits_remaining -= 2;
                    if (bits_remaining == 0) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
                    uint8_t z_bits = bit_buffer & 0x03;
                    bit_buffer >>= 2;
                    bits_remaining -= 2;
                    if (bits_remaining == 0) {
                        bit_buffer = input[input_pos++];
                        bits_remaining = 8;
                    }
                    
                    // Reconstruct vector from reference and difference
                    output[output_pos].x = (reference[output_pos].x & 0xfc) + x_bits;
                    output[output_pos].y = (reference[output_pos].y & 0xfc) + y_bits;
                    output[output_pos].z = (reference[output_pos].z & 0xfc) + z_bits;
                    output_pos++;
                }
                break;
            }
        }
    }
    
    return output_pos;
}


void calculate_errors(const size_t num, Vector3D* input, Vector3D* decompressed) {
    // Calculate mean quadratic difference between input and decompressed data
    double sum_diff_x = 0.0, sum_diff_y = 0.0, sum_diff_z = 0.0;

    for (size_t i = 0; i < num; i++) {
        // Calculate squared differences for each dimension
        double diff_x = (double)input[i].x - (double)decompressed[i].x;
        double diff_y = (double)input[i].y - (double)decompressed[i].y;
        double diff_z = (double)input[i].z - (double)decompressed[i].z;
    
        double sum_diff_x_d = diff_x * diff_x;
        double sum_diff_y_d = diff_y * diff_y;
        double sum_diff_z_d = diff_z * diff_z;

        sum_diff_x += diff_x * diff_x;
        sum_diff_y += diff_y * diff_y;
        sum_diff_z += diff_z * diff_z;

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

int main(int argc, char *argv[]) {
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
        for (size_t i=0; i < 5; i++) {

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
