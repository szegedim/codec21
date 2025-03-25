// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above
/*
gcc -o b.out receiver.c codec21.c display.c -lX11 -lImlib2 && ./b.out
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "codec21.h"
#include "display.h"

#define PORT 14550
#define BUFFER_SIZE (WIDTH * 3 * 2 + 1 + 1)
#define STRIDE (WIDTH * 3 + 1)

typedef struct {
    uint8_t* input;
    Vector3D* output;
    volatile size_t current_line;
    volatile size_t current_pos;
    // Statistics tracking
    size_t total_bytes_received;
    size_t total_pixels_decoded;
} BufferPair;

static BufferPair buffer_pairs[2];
static volatile int active_pair = 0;

void switch_buffer_pair(void) {
    // No need to print statistics here since they're printed before display
    printf("Switching buffers - current line: %zu\n", buffer_pairs[active_pair].current_line);
    active_pair = 1 - active_pair;
    
    // Reset statistics for the new active buffer
    buffer_pairs[active_pair].current_line = 0;
    buffer_pairs[active_pair].current_pos = 0;
    buffer_pairs[active_pair].total_bytes_received = 0;
    buffer_pairs[active_pair].total_pixels_decoded = 0;
    
    memset(buffer_pairs[active_pair].input, 0, STRIDE * HEIGHT);
}

void process_udp_buffer(const uint8_t* packet_buffer, size_t packet_length) {
    BufferPair* current = &buffer_pairs[active_pair];
    BufferPair* other = &buffer_pairs[1 - active_pair];
    
    // Track total bytes received for this frame (excluding frame end marker)
    if (!(packet_length == 1 && packet_buffer[0] == '\t')) {
        current->total_bytes_received += packet_length;
    }

    if (packet_length == 1 && packet_buffer[0] == '\t') {
        printf("Frame end marker detected (\\t)\n");
        
        // Display frame after all data is received
        if (current->current_line >= HEIGHT) {
            // Print collected statistics before displaying the frame
            printf("Frame statistics: %zu compressed bytes received, %zu lines decoded, %zu pixels decompressed (%.2f bytes per pixel)\n", 
                current->total_bytes_received,
                current->current_line,
                current->total_pixels_decoded,
                (float)current->total_bytes_received / (current->total_pixels_decoded > 0 ? current->total_pixels_decoded : 1));
            
            printf("All lines processed, displaying frame\n");
            display_frame(current->output);
            printf("Frame displayed, switching buffers\n");
            switch_buffer_pair();
        }
        return;
    }

    if (current->current_line >= HEIGHT) {
        return;
    }

    uint8_t* line_start = current->input + (current->current_line * STRIDE);
    
    size_t process_length = packet_length;
    if (packet_length > 0 && packet_buffer[packet_length - 1] == '\v') {
        process_length--;
    }

    for (size_t i = 0; i < process_length; i++) {
        if (current->current_pos < STRIDE) {
            line_start[current->current_pos++] = packet_buffer[i];
        }
    }

    if (packet_length > 0 && packet_buffer[packet_length - 1] == '\v') {
        // Line completed - decode it immediately
        uint8_t* input_line = current->input + (current->current_line * STRIDE);
        Vector3D* output_line = current->output + (current->current_line * WIDTH);
        Vector3D* reference_line = other->output + (current->current_line * WIDTH);
        
        size_t pixels_decompressed = decode_blocks(input_line, STRIDE, output_line, reference_line);
        
        // Track pixels decoded for statistics without printing per-line info
        current->total_pixels_decoded += pixels_decompressed;
        
        // Move to next line without printing per-line statistics
        current->current_line++;
        current->current_pos = 0;
    }
}

int main(void) {
    for (int i = 0; i < 2; i++) {
        buffer_pairs[i].input = (uint8_t*)calloc(STRIDE * HEIGHT, sizeof(uint8_t));
        buffer_pairs[i].output = (Vector3D*)calloc(WIDTH * HEIGHT, sizeof(Vector3D));
        buffer_pairs[i].current_line = 0;
        buffer_pairs[i].current_pos = 0;
        
        // Initialize statistics
        buffer_pairs[i].total_bytes_received = 0;
        buffer_pairs[i].total_pixels_decoded = 0;
    }

    // Initialize display
    if (init_display() == 0) {
        fprintf(stderr, "Failed to initialize display\n");
        return 1;
    }
    printf("Display initialized successfully\n");

    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sock_fd);
        return 1;
    }

    printf("UDP server listening on port %d\n", PORT);

    while (1) {
        uint8_t packet_buffer[BUFFER_SIZE];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        ssize_t packet_length = recvfrom(sock_fd, packet_buffer, BUFFER_SIZE, 0,
                                       (struct sockaddr*)&client_addr, &client_len);
        
        if (packet_length < 0) {
            perror("recvfrom failed");
            continue;
        }

        process_udp_buffer(packet_buffer, packet_length);
    }

    return 0;
}