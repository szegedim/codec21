// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

/*
gcc -o sender.out sender.c codec21.c -lImlib2 && ./sender.out
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <Imlib2.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "codec21.h"

// Increasing it to verify lossless compression quality.
#define TEST_DELAY 1
#define UDP_PORT 14721

typedef struct {
    char *name;
    long long number;  // Using long long for microsecond timestamps
} ImageFile;

int running = 1;
int sockfd;
struct sockaddr_in server_addr;

int compare(const void *a, const void *b) {
    long long diff = ((ImageFile*)a)->number - ((ImageFile*)b)->number;
    return (diff > 0) - (diff < 0);
}

// Function to convert Imlib_Image to Vector3D array
Vector3D* image_to_vector3d(Imlib_Image img, int *size) {
    int width, height;
    imlib_context_set_image(img);
    width = imlib_image_get_width();
    height = imlib_image_get_height();
    *size = width * height;
    
    Vector3D* data = malloc(*size * sizeof(Vector3D));
    if (!data) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    
    DATA32* pixels = imlib_image_get_data();
    for (int i = 0; i < *size; i++) {
        data[i].x = (pixels[i] >> 16) & 0xFF; // Red
        data[i].y = (pixels[i] >> 8) & 0xFF;  // Green
        data[i].z = pixels[i] & 0xFF;         // Blue
    }
    
    return data;
}

// Function to send data over UDP
void send_udp(uint8_t* data, size_t size) {
    if (sendto(sockfd, data, size, 0, 
              (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("UDP send failed");
    }
}

void *process_images(void *arg) {
    DIR *dir;
    struct dirent *entry;
    ImageFile *files = NULL;
    int file_count = 0;
    
    dir = opendir(".");
    if (!dir) {
        fprintf(stderr, "Cannot open directory\n");
        return NULL;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".png") == 0) {
            char *number_str = strdup(entry->d_name);
            number_str[ext - entry->d_name] = '\0';
            
            char *endptr;
            long long num = strtoll(number_str, &endptr, 10);
            
            if (*endptr == '\0') {  // Valid number
                files = realloc(files, (file_count + 1) * sizeof(ImageFile));
                files[file_count].name = strdup(entry->d_name);
                files[file_count].number = num;
                file_count++;
            }
            free(number_str);
        }
    }
    closedir(dir);
    
    if (file_count == 0) {
        fprintf(stderr, "No matching PNG files found\n");
        return NULL;
    }
    
    printf("Found %d matching files\n", file_count);
    qsort(files, file_count, sizeof(ImageFile), compare);
    
    // Allocate memory for reference frame
    Vector3D* reference_frame = calloc(WIDTH * HEIGHT, sizeof(Vector3D));
    if (!reference_frame) {
        fprintf(stderr, "Failed to allocate memory for reference frame\n");
        return NULL;
    }
    
    printf("Starting continuous processing loop. Press Ctrl+C to quit...\n");

    while (running) {
        // Modified loop to process each file for eight frames before moving to the next
        for (int i = 0; i < file_count && running; i++) {
            // Load the image once
            printf("Loading file %s\n", files[i].name);
            Imlib_Image img = imlib_load_image(files[i].name);
            if (!img) {
                printf("Failed to load image %s, skipping\n", files[i].name);
                continue;
            }
            
            int image_size;
            Vector3D* image_data = image_to_vector3d(img, &image_size);
            imlib_free_image(); // Free the image after converting to Vector3D
            
            if (!image_data) {
                printf("Failed to convert image %s to vector data, skipping\n", files[i].name);
                continue;
            }
            
            // Process the same image for TEST_DELAY frames
            for (int frame = 0; frame < TEST_DELAY && running; frame++) {
                printf("Processing %s (frame %d of %d)\n", files[i].name, frame + 1, TEST_DELAY);
                
                int width = WIDTH;
                int height = HEIGHT;
                
                uint8_t* temp_buffer = malloc(width * sizeof(Vector3D) * 2 + 1); // +1 for separator
                Vector3D* reference_frame_copy = malloc(WIDTH * HEIGHT * sizeof(Vector3D));
                
                if (temp_buffer && reference_frame_copy) {
                    size_t total_bytes_compressed = 0;
                    size_t total_bytes_decompressed = 0;
                    size_t total_compressible_size = 0;
                    
                    // Make a copy of the reference frame at the start of frame processing
                    memcpy(reference_frame_copy, reference_frame, WIDTH * HEIGHT * sizeof(Vector3D));
                    
                    for (int line = 0; line < height; line++) {
                        int segment_width = width / 4; // Width of each segment
                        
                        for (int chunk = 0; chunk < 4; chunk++) {
                            // Calculate the starting position for this chunk
                            int start_pos = line * width + chunk * segment_width;
                            int current_segment_width = (chunk < 3) ? segment_width : width - (3 * segment_width);
                            
                            // Track the compressible size in bytes
                            total_compressible_size += current_segment_width * sizeof(Vector3D);
                            
                            size_t chunk_compressed_size = encode_block(
                                &image_data[start_pos],
                                &reference_frame_copy[start_pos],
                                current_segment_width,
                                temp_buffer,
                                current_segment_width * sizeof(Vector3D) * 2
                            );
                            
                            total_bytes_compressed += chunk_compressed_size;
                            
                            // Send the compressed block over UDP
                            send_udp(temp_buffer, chunk_compressed_size);
                            
                            size_t chunk_decompressed_size = decode_blocks(
                                temp_buffer,
                                chunk_compressed_size,
                                &reference_frame[start_pos],
                                &reference_frame_copy[start_pos]
                            );
                            
                            total_bytes_decompressed += chunk_decompressed_size * sizeof(Vector3D);
                            
                            // If this is the last chunk of the line, send a line ending marker
                            if (chunk == 3) {
                                uint8_t line_marker = '\v';  // Vertical tab as line ending marker
                                send_udp(&line_marker, 1);
                            }
                        }
                    }
                    
                    // Send a terminating tab character to signal end of frame
                    uint8_t terminator = '\t';
                    send_udp(&terminator, 1);
                    
                    double compression_ratio = (double)total_compressible_size / (double)total_bytes_compressed;
                    printf("Frame statistics:\n");
                    printf("  Compressed size: %zu bytes\n", total_bytes_compressed);
                    printf("  Decompressed size: %zu bytes\n", total_bytes_decompressed);
                    printf("  Compressible size: %zu bytes\n", total_compressible_size);
                    printf("  Compression ratio: %.2f:1\n", compression_ratio);
                    if (total_compressible_size != total_bytes_decompressed) {
                        exit(1);
                    }
                    
                    // Free temporary buffers
                    free(reference_frame_copy);
                    free(temp_buffer);
                } else {
                    if (temp_buffer) free(temp_buffer);
                    if (reference_frame_copy) free(reference_frame_copy);
                    fprintf(stderr, "Failed to allocate buffers for encoding\n");
                }
                
                // Add delay between frames
                usleep(30000); // 30ms delay between frames of the same image
            }
            
            // Free the image data after all frames are processed
            free(image_data);
            
            if (i < file_count - 1) {
                // Add delay between different images
                long long delay_us = files[i + 1].number - files[i].number;
                if (delay_us > 0) {
                    printf("Delaying before next file: %lld microseconds\n", delay_us);
                    usleep((useconds_t)delay_us);
                }
            } else {
                // When we reach the last file, small delay before looping back to first file
                printf("Reached last file, looping back to beginning\n");
                usleep(30000);  // 30ms delay before restarting
            }
        }
    }
    
    // Clean up
    free(reference_frame);
    for (int i = 0; i < file_count; i++) {
        free(files[i].name);
    }
    free(files);
    
    return NULL;
}

int main() {
    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Sending to localhost
    
    printf("UDP sender initialized, sending to port %d\n", UDP_PORT);
    
    // Create thread for processing images
    pthread_t processing_thread;
    if (pthread_create(&processing_thread, NULL, process_images, NULL) != 0) {
        perror("Failed to create processing thread");
        running = 0;
        close(sockfd);
        return 1;
    }
    
    printf("Image processing started\n");
    
    // Wait for thread to finish (it only exits on program termination)
    pthread_join(processing_thread, NULL);
    
    // Close socket
    close(sockfd);
    printf("Socket closed, program terminating\n");
    
    return 0;
}