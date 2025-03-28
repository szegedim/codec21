// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

/*
gcc -o receiver.out receiver.c codec21.c display.c -lX11 -lImlib2 && ./receiver.out
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "codec21.h"
#include "display.h"

#define UDP_PORT 14721
#define MAX_PACKET_SIZE 65536  // Maximum UDP packet size

int running = 1;

typedef struct {
    int segments_received;
    int total_segments;
    Vector3D* reference_frame;
    Vector3D* reference_frame_copy;
} FrameState;

void *receive_and_process(void *arg) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    uint8_t buffer[MAX_PACKET_SIZE];
    
    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        running = 0;
        return NULL;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_PORT);
    
    // Bind socket
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        running = 0;
        return NULL;
    }
    
    printf("UDP receiver listening on port %d\n", UDP_PORT);
    
    // Allocate memory for reference frame and its copy
    Vector3D* reference_frame = calloc(WIDTH * HEIGHT, sizeof(Vector3D));
    Vector3D* reference_frame_copy = malloc(WIDTH * HEIGHT * sizeof(Vector3D));
    
    if (!reference_frame || !reference_frame_copy) {
        fprintf(stderr, "Failed to allocate memory for reference frames\n");
        free(reference_frame);
        free(reference_frame_copy);
        close(sockfd);
        running = 0;
        return NULL;
    }
    
    int segments_per_frame = WIDTH * HEIGHT / (WIDTH/4);  // Total segments in a frame
    int segments_received = 0;
    size_t total_bytes_decompressed = 0;
    
    printf("Ready to receive. Each frame consists of %d segments\n", segments_per_frame);
    
    int waiting_for_terminator = 1;  // Start by waiting for a terminator
    
    while (running) {
        // Receive a UDP packet
        int packet_size = recvfrom(sockfd, buffer, MAX_PACKET_SIZE, 0,
                                  (struct sockaddr*)&client_addr, &client_len);
        
        if (packet_size <= 0) {
            continue;  // Skip empty packets
        }
        
        // Check if this is a terminator packet
        if (packet_size == 1 && buffer[0] == '\t') {
            if (!waiting_for_terminator) {
                // We've reached the end of a frame
                printf("Frame terminator received (%zu bytes decompressed)\n", total_bytes_decompressed);
                
                // Only display the frame if we've received the expected amount of data
                if (total_bytes_decompressed >= WIDTH * HEIGHT * 3) {
                    printf("Complete frame received, displaying\n");
                    // Display the fully decoded frame
                    display_frame(reference_frame);
                } else {
                    printf("Incomplete frame received, discarding (%zu bytes of %d expected)\n", 
                           total_bytes_decompressed, WIDTH * HEIGHT * 3);
                }
                
                // Reset for next frame and wait for next terminator
                segments_received = 0;
                waiting_for_terminator = 1;
            } else {
                // We received a terminator when already waiting - this means we can start receiving a new frame
                waiting_for_terminator = 0;
                
                // Reset for new frame
                memcpy(reference_frame_copy, reference_frame, WIDTH * HEIGHT * sizeof(Vector3D));
                total_bytes_decompressed = 0;
                segments_received = 0;
            }
            continue;  // Skip to next packet
        }
        
        // Check if this is a line ending marker
        if (packet_size == 1 && buffer[0] == '\v') {
            // Calculate if we're at the end of a line (after 4 chunks)
            int expected_segment_at_line_end = (segments_received % 4 == 0);
            
            if (!expected_segment_at_line_end) {
                printf("Line ending marker received out of sequence, resetting to wait for new frame\n");
                // Reset and wait for a new frame
                segments_received = 0;
                waiting_for_terminator = 1;
            } else {
                printf("Line ending marker received correctly\n");
            }
            continue;  // Skip to next packet
        }
        
        // If we're waiting for a terminator, ignore all other packets
        if (waiting_for_terminator) {
            continue;
        }
        
        // If this is the first segment of a new frame, make a copy of the reference frame
        if (segments_received == 0) {
            memcpy(reference_frame_copy, reference_frame, WIDTH * HEIGHT * sizeof(Vector3D));
            total_bytes_decompressed = 0;
        }
        
        // Calculate which segment this is (for debugging)
        int segment_number = segments_received % segments_per_frame;
        int line = segment_number / 4;
        int chunk = segment_number % 4;
        
        // Calculate position in the frame
        int segment_width = WIDTH / 4;
        int start_pos = line * WIDTH + chunk * segment_width;
        int current_segment_width = (chunk < 3) ? segment_width : WIDTH - (3 * segment_width);
        
        // Decode this segment
        size_t chunk_decompressed_size = decode_blocks(
            buffer,
            packet_size,
            &reference_frame[start_pos],
            &reference_frame_copy[start_pos]
        );
        
        total_bytes_decompressed += chunk_decompressed_size * sizeof(Vector3D);
        segments_received++;
    }
    
    // Clean up
    free(reference_frame);
    free(reference_frame_copy);
    close(sockfd);
    
    return NULL;
}

int main() {
    // Initialize display
    if (init_display() == 0) {
        fprintf(stderr, "Failed to initialize display\n");
        return 1;
    }
    printf("Display initialized successfully\n");
    
    // Create thread for receiving and processing
    pthread_t processing_thread;
    if (pthread_create(&processing_thread, NULL, receive_and_process, NULL) != 0) {
        perror("Failed to create processing thread");
        running = 0;
        cleanup_display();
        return 1;
    }
    
    printf("Receiver started\n");
    
    // Wait for thread to finish (it only exits on program termination)
    pthread_join(processing_thread, NULL);
    
    // Clean up display resources
    cleanup_display();
    printf("Display resources cleaned up\n");
    
    return 0;
}