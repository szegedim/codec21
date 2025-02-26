
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 14550    // UDP port to listen on
#define BUFFER_SIZE 1024
#define STRIDE (1920 * 4 + 1)  // Fixed stride for 1920 pixels * 4 bytes + 1
#define HEIGHT 1080            // Fixed height for 1080p resolution

void process_udp_buffer(uint8_t* dest_buffer, size_t stride, size_t height, 
                       const uint8_t* packet_buffer, size_t packet_length);

int main(void) {
    // Allocate destination buffer
    uint8_t* dest_buffer = (uint8_t*)calloc(STRIDE * HEIGHT, sizeof(uint8_t));
    if (!dest_buffer) {
        perror("Failed to allocate destination buffer");
        return 1;
    }

    // Create UDP socket
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        free(dest_buffer);
        return 1;
    }

    // Configure socket address
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    // Bind socket
    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sock_fd);
        free(dest_buffer);
        return 1;
    }

    printf("Listening on UDP port %d (Stride: %d, Height: %d)...\n", 
           PORT, STRIDE, HEIGHT);

    // Receive packets
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

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Received %zd bytes from %s:%d\n", 
               packet_length, client_ip, ntohs(client_addr.sin_port));

        // Process the received packet
        process_udp_buffer(dest_buffer, STRIDE, HEIGHT, packet_buffer, packet_length);
    }

    // Cleanup (never reached in this example)
    close(sock_fd);
    free(dest_buffer);
    return 0;
}

void process_udp_buffer(uint8_t* dest_buffer, size_t stride, size_t height, 
                       const uint8_t* packet_buffer, size_t packet_length) {
    static size_t current_line = 0;
    static size_t current_pos = 0;

    // Check if we've exceeded the vertical bounds
    if (current_line >= height) {
        return;
    }

    // Check for horizontal tab (termination character)
    if (packet_length == 1 && packet_buffer[0] == '\t') {
        return;
    }

    // Calculate the start position for the current line
    uint8_t* line_start = dest_buffer + (current_line * stride);

    // Process each byte in the packet except the last one
    size_t process_length = packet_length;
    if (packet_length > 0 && packet_buffer[packet_length - 1] == '\v') {
        process_length--; // Don't process the vertical tab in the main loop
    }

    // Process regular data
    for (size_t i = 0; i < process_length; i++) {
        // Only write if we haven't exceeded the stride
        if (current_pos < stride) {
            line_start[current_pos++] = packet_buffer[i];
        }
    }

    // Handle vertical tab if it's the last character
    if (packet_length > 0 && packet_buffer[packet_length - 1] == '\v') {
        current_line++;
        current_pos = 0;
    }
}