// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
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
} BufferPair;

static BufferPair buffer_pairs[2];
static volatile int active_pair = 0;
volatile int kanban = 0;

typedef struct {
    int thread_id;
} ThreadParams;

void switch_buffer_pair(void) {
    // Print the current line before switching buffers
    printf("Switching buffers - current line: %zu\n", buffer_pairs[active_pair].current_line);
    if (kanban) {
        
        active_pair = 1 - active_pair;
        buffer_pairs[active_pair].current_line = 0;
        buffer_pairs[active_pair].current_pos = 0;
        memset(buffer_pairs[active_pair].input, 0, STRIDE * HEIGHT);
        kanban = 0;
    }
}

void* decoder_thread(void* arg) {
    ThreadParams* params = (ThreadParams*)arg;
    int thread_id = params->thread_id;
    
    while (1) {
        BufferPair* current = &buffer_pairs[active_pair];
        BufferPair* other = &buffer_pairs[1 - active_pair];
        size_t line = thread_id;

        while (line < HEIGHT) {
            if (line < current->current_line) {
                uint8_t* input_line = current->input + (line * STRIDE);
                Vector3D* output_line = current->output + (line * WIDTH);
                Vector3D* reference_line = other->output + (line * WIDTH);
                
                size_t pixels_decompressed = decode_blocks(input_line, STRIDE, output_line, reference_line);
                
                // Print information about decompression result
                printf("Thread %d: Decoded line %zu (%zu pixels decompressed)\n", 
                       thread_id, line, pixels_decompressed);
                
                line += 2;
            }
            
            // Check if all lines are processed
            // Only thread 0 should set kanban to avoid race conditions
            if (thread_id == 0 && current->current_line >= HEIGHT) {
                // If we've processed all lines and received all data for this frame
                printf("All lines processed by decoder thread, setting kanban\n");
                kanban = 1;
                // Allow some time for display before switching
                usleep(16000); // ~16ms (60fps)
            }
            
            // Small sleep to prevent CPU hogging
            usleep(1000);
        }
    }
    return NULL;
}

void process_udp_buffer(const uint8_t* packet_buffer, size_t packet_length) {
    // Print info about every received packet
    printf("Received packet with length %zu bytes%s\n", 
           packet_length,
           (packet_length > 0 && packet_buffer[packet_length - 1] == '\v') ? " (with line separator \\v)" : "");
    
    BufferPair* current = &buffer_pairs[active_pair];

    if (packet_length == 1 && packet_buffer[0] == '\t') {
        printf("Frame end marker detected (\\t)\n");
        switch_buffer_pair();
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
        // Line completed
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
    }

    pthread_t decoder_threads[2];
    ThreadParams thread_params[2] = {{0}, {1}};
    
    for (int i = 0; i < 2; i++) {
        pthread_create(&decoder_threads[i], NULL, decoder_thread, &thread_params[i]);
    }

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