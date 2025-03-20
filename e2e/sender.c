// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Imlib2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "codec21.h"

#define PORT 14550

typedef struct {
    char *name;
    long long number;  // Using long long for microsecond timestamps
} ImageFile;

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

int main() {
    // Initialize UDP socket
    int sockfd;
    struct sockaddr_in server_addr;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
    
    DIR *dir;
    struct dirent *entry;
    ImageFile *files = NULL;
    int file_count = 0;
    
    dir = opendir(".");
    if (!dir) {
        fprintf(stderr, "Cannot open directory\n");
        return 1;
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
        return 1;
    }
    
    printf("Found %d matching files\n", file_count);
    qsort(files, file_count, sizeof(ImageFile), compare);
    
    // Allocate memory for reference frame only (remove reference_frame_copy from here)
    Vector3D* reference_frame = calloc(WIDTH * HEIGHT, sizeof(Vector3D));
    if (!reference_frame) {
        fprintf(stderr, "Failed to allocate memory for reference frame\n");
        return 1;
    }
    
    printf("Starting continuous processing loop. Press Ctrl+C to quit...\n");

    int running = 1;
    
    while (running) {
        for (int i = 0; i < file_count && running; i++) {
            printf("Processing %s\n", files[i].name);
            
            Imlib_Image img = imlib_load_image(files[i].name);
            if (img) {
                int image_size;
                Vector3D* image_data = image_to_vector3d(img, &image_size);
                
                if (image_data) {
                    int width = WIDTH;
                    int height = HEIGHT;
                    
                    size_t max_compressed_size = image_size * sizeof(Vector3D) * 2 + height;
                    uint8_t* compressed_data = malloc(max_compressed_size);
                    // Modify the temp_buffer allocation to include space for the separator
                    uint8_t* temp_buffer = malloc(width * sizeof(Vector3D) * 2 + 1); // +1 for separator
                    
                    // Allocate memory for reference_frame_copy
                    Vector3D* reference_frame_copy = malloc(WIDTH * HEIGHT * sizeof(Vector3D));
                    
                    if (compressed_data && temp_buffer && reference_frame_copy) {
                        size_t compressed_size = 0;
                        
                        // Make a copy of the reference frame at the start of each frame processing
                        memcpy(reference_frame_copy, reference_frame, WIDTH * HEIGHT * sizeof(Vector3D));
                        
                        for (int line = 0; line < height; line++) {
                            size_t line_compressed_size = encode_block(
                                &image_data[line * width], 
                                &reference_frame_copy[line * width], 
                                width, 
                                temp_buffer, 
                                width * sizeof(Vector3D) * 2
                            );
                            
                            temp_buffer[line_compressed_size] = '\v';
                            
                            sendto(sockfd, temp_buffer, line_compressed_size + 1, 0,
                                  (struct sockaddr*)&server_addr, sizeof(server_addr));

                            decode_blocks(
                                temp_buffer,
                                line_compressed_size,
                                &reference_frame[line * width],
                                &reference_frame_copy[line * width]
                            );

                            printf("Sent line %d (%zu bytes + separator)\n", line, line_compressed_size);
                            
                            usleep(500);
                        }
                        
                        uint8_t frame_end_marker = '\t';
                        sendto(sockfd, &frame_end_marker, 1, 0,
                              (struct sockaddr*)&server_addr, sizeof(server_addr));
                        
                        printf("Sent frame end marker (\\t)\n");
                        
                        memcpy(reference_frame, image_data, image_size * sizeof(Vector3D));
                        
                        // Free reference_frame_copy before exiting this block
                        free(reference_frame_copy);
                        free(compressed_data);
                        free(temp_buffer);
                    } else {
                        // Handle allocation failures
                        if (compressed_data) free(compressed_data);
                        if (temp_buffer) free(temp_buffer);
                        if (reference_frame_copy) free(reference_frame_copy);
                        fprintf(stderr, "Failed to allocate buffers for encoding\n");
                    }
                    
                    free(image_data);
                }
                
                imlib_free_image();
            }

            if (i < file_count - 1) {
                long long delay_us = files[i + 1].number - files[i].number;
                if (delay_us > 0) {
                    printf("Delaying for %lld microseconds\n", delay_us);
                    usleep((useconds_t)delay_us);
                }
            } else {
                // When we reach the last file, small delay before looping back to first file
                printf("Reached last file, looping back to beginning\n");
                usleep(30000);  // 30ms delay before restarting
            }
        }
    }
    
    free(reference_frame);
    for (int i = 0; i < file_count; i++) {
        free(files[i].name);
    }
    free(files);
    close(sockfd);
    
    return 0;
}