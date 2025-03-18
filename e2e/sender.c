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

// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

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
    
    // Allocate memory for reference frame
    Vector3D* reference_frame = calloc(WIDTH * HEIGHT, sizeof(Vector3D));
    if (!reference_frame) {
        fprintf(stderr, "Failed to allocate memory for reference frame\n");
        return 1;
    }
    
    int running = 1;
    char input_buffer[10];
    
    printf("Press Enter to start processing or 'q' to quit...\n");
    while (running) {
        // Non-blocking check for input
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        
        if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
            if (fgets(input_buffer, sizeof(input_buffer), stdin)) {
                if (input_buffer[0] == 'q' || input_buffer[0] == 'Q') {
                    running = 0;
                    break;
                }
            }
        }
        
        for (int i = 0; i < file_count && running; i++) {
            printf("Processing %s\n", files[i].name);
            
            Imlib_Image img = imlib_load_image(files[i].name);
            if (img) {
                int image_size;
                Vector3D* image_data = image_to_vector3d(img, &image_size);
                
                if (image_data) {
                    // Allocate memory for compressed data (worst case: slightly larger than original)
                    size_t max_compressed_size = image_size * sizeof(Vector3D) * 2;
                    uint8_t* compressed_data = malloc(max_compressed_size);
                    
                    if (compressed_data) {
                        // Encode the image
                        size_t compressed_size = encode_block(
                            image_data, reference_frame, 
                            image_size, compressed_data, max_compressed_size
                        );
                        
                        printf("Compressed size: %zu bytes\n", compressed_size);
                        
                        // Send the compressed data via UDP
                        if (compressed_size > 0) {
                            // Send data in chunks to respect UDP packet size limits
                            const size_t MAX_CHUNK_SIZE = 1400; // Safe UDP packet size
                            
                            // Create a properly sized header with frame number and total size
                            // Using uint32_t for frame number and uint32_t for size to ensure consistent size
                            uint32_t header[3]; // 12 bytes total: frame number, size high bits, size low bits
                            
                            // Store frame number
                            header[0] = (uint32_t)i;
                            
                            // Split compressed_size into two 32-bit parts to ensure portability
                            header[1] = (uint32_t)(compressed_size & 0xFFFFFFFF);         // Low 32 bits
                            header[2] = (uint32_t)((compressed_size >> 32) & 0xFFFFFFFF); // High 32 bits
                            
                            // Send the header
                            sendto(sockfd, header, sizeof(header), 0, 
                                  (struct sockaddr*)&server_addr, sizeof(server_addr));
                            
                            // Then send the actual data in chunks
                            for (size_t offset = 0; offset < compressed_size; offset += MAX_CHUNK_SIZE) {
                                size_t chunk_size = compressed_size - offset;
                                if (chunk_size > MAX_CHUNK_SIZE) chunk_size = MAX_CHUNK_SIZE;
                                
                                sendto(sockfd, compressed_data + offset, chunk_size, 0, 
                                      (struct sockaddr*)&server_addr, sizeof(server_addr));
                                
                                // Small delay to avoid overwhelming the receiver
                                usleep(1000);
                            }
                        }
                        
                        // Update reference frame for next iteration
                        memcpy(reference_frame, image_data, image_size * sizeof(Vector3D));
                        
                        free(compressed_data);
                    }
                    
                    free(image_data);
                }
                
                imlib_free_image();
            }
            
            // Calculate delay to next timestamp (except for last image)
            if (i < file_count - 1) {
                long long delay_us = files[i + 1].number - files[i].number;
                if (delay_us > 0) {
                    usleep((useconds_t)delay_us);
                }
            } else {
                usleep(30000);  // Default delay for last image
            }
            
            // Non-blocking check for 'q' key to quit
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            
            if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0) {
                if (fgets(input_buffer, sizeof(input_buffer), stdin)) {
                    if (input_buffer[0] == 'q' || input_buffer[0] == 'Q') {
                        running = 0;
                        break;
                    }
                }
            }
        }
        
        // At the end of all files, exit the loop unless we want to repeat
        running = 0;
        printf("Processing complete. Press Enter to restart or 'q' to quit...\n");
        fgets(input_buffer, sizeof(input_buffer), stdin);
        if (input_buffer[0] != 'q' && input_buffer[0] != 'Q') {
            running = 1;
        }
    }
    
    // Clean up
    free(reference_frame);
    for (int i = 0; i < file_count; i++) {
        free(files[i].name);
    }
    free(files);
    close(sockfd);
    
    return 0;
}