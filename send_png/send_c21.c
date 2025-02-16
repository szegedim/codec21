#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <png.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8888
#define MAX_UDP_PAYLOAD 1200
#define PIXELS_PER_CHUNK 300
#define RGB_BYTES_PER_PIXEL 3

int main() {
    int sock;
    struct sockaddr_in server_addr;
    png_bytep row = NULL;
    unsigned char *chunk_buffer = NULL;
    uint16_t chunk_ordinal = 0;

    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Open PNG file
    FILE *fp = fopen("tulip.png", "rb");
    if (!fp) {
        perror("Failed to open PNG file");
        close(sock);
        return 1;
    }

    // Initialize PNG reading
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        close(sock);
        return 1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        close(sock);
        return 1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        close(sock);
        return 1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    // Transform PNG to RGB if needed
    if (bit_depth == 16)
        png_set_strip_16(png);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    // Allocate memory
    row = malloc(png_get_rowbytes(png, info));
    chunk_buffer = malloc(2 + PIXELS_PER_CHUNK * RGB_BYTES_PER_PIXEL + 1); // +2 for ordinal, +1 for \v

    if (!row || !chunk_buffer) {
        perror("Memory allocation failed");
        goto cleanup;
    }

    printf("Processing PNG: %dx%d pixels\n", width, height);

    // Process image row by row
    for (int y = 0; y < height; y++) {
        png_read_row(png, row, NULL);
        int pixels_remaining = width;
        int x = 0;

        while (pixels_remaining > 0) {
            int pixels_this_chunk = (pixels_remaining < PIXELS_PER_CHUNK) ? 
                                   pixels_remaining : PIXELS_PER_CHUNK;
            size_t chunk_size = 2; // Start after ordinal

            // Write ordinal
            chunk_buffer[0] = (chunk_ordinal >> 8) & 0xFF;
            chunk_buffer[1] = chunk_ordinal & 0xFF;

            // Copy RGB data, skipping alpha channel
            for (int i = 0; i < pixels_this_chunk; i++) {
                // Explicitly copy R,G,B components
                chunk_buffer[chunk_size++] = row[(x + i) * 4 + 0]; // R
                chunk_buffer[chunk_size++] = row[(x + i) * 4 + 1]; // G
                chunk_buffer[chunk_size++] = row[(x + i) * 4 + 2]; // B
            }

            // Add vertical tab for end of row
            if (pixels_remaining <= PIXELS_PER_CHUNK) {
                chunk_buffer[chunk_size++] = '\v';
            }

            // Send chunk
            if (sendto(sock, chunk_buffer, chunk_size, 0,
                      (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("Failed to send chunk");
                goto cleanup;
            }

            printf("Sent chunk %d: %d pixels\n", chunk_ordinal, pixels_this_chunk);
            chunk_ordinal++;
            //usleep(1000);  // Small delay between chunks

            pixels_remaining -= pixels_this_chunk;
            x += pixels_this_chunk;
        }
    }

    // Send tab character with ordinal to signal completion
    unsigned char end_frame[3];
    end_frame[0] = (chunk_ordinal >> 8) & 0xFF;
    end_frame[1] = chunk_ordinal & 0xFF;
    end_frame[2] = '\t';
    
    if (sendto(sock, end_frame, 3, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to send completion signal");
        goto cleanup;
    }
    printf("Sent completion signal (ordinal %d with tab character)\n", chunk_ordinal);
    chunk_ordinal = 0;

cleanup:
    free(row);
    free(chunk_buffer);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    close(sock);
    return 0;
}