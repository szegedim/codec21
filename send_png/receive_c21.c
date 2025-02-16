#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <png.h>
#include <time.h>

#define PORT 8888
#define MAX_UDP_PAYLOAD 1200
#define MAX_WIDTH 1920
#define MAX_HEIGHT 1080
#define RGB_BYTES_PER_PIXEL 3
#define MAX_CHUNKS 65536  // Maximum possible 16-bit ordinals

typedef struct {
    unsigned char *data;
    size_t size;
    int received;
} ChunkEntry;

typedef struct {
    unsigned char *data;
    int width;
    int height;
    int current_x;
    int current_y;
} ImageBuffer;

void process_chunks_to_image(ImageBuffer *img, ChunkEntry *chunks, uint16_t max_ordinal) {
    img->current_x = 0;
    img->current_y = 0;

    for (uint16_t i = 0; i < max_ordinal; i++) {
        if (!chunks[i].received) {
            printf("Warning: Missing chunk %d\n", i);
            continue;
        }

        unsigned char *chunk_data = chunks[i].data;
        size_t chunk_size = chunks[i].size;

        // Skip ordinal bytes
        size_t pos = 2;
        while (pos < chunk_size) {
            if (chunk_data[pos] == '\v') {
                img->current_x = 0;
                img->current_y++;
                pos++;
                continue;
            }

            if (img->current_x < img->width && img->current_y < img->height) {
                size_t pixel_offset = (img->current_y * img->width + img->current_x) * RGB_BYTES_PER_PIXEL;
                memcpy(img->data + pixel_offset, chunk_data + pos, RGB_BYTES_PER_PIXEL);
                pos += RGB_BYTES_PER_PIXEL;
                img->current_x++;
            }
        }
    }
}

void write_png(ImageBuffer *img, int frame_number) {
    char filename[32];
    snprintf(filename, sizeof(filename), "frame_%d.png", frame_number);
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("Failed to open output file");
        return;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, img->width, img->height, 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_bytep *row_pointers = malloc(sizeof(png_bytep) * img->height);
    for (int y = 0; y < img->height; y++) {
        row_pointers[y] = img->data + (y * img->width * RGB_BYTES_PER_PIXEL);
    }

    png_write_info(png, info);
    png_write_image(png, row_pointers);
    png_write_end(png, NULL);

    free(row_pointers);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    printf("Wrote %s\n", filename);
}

void cleanup_chunks(ChunkEntry *chunks, uint16_t max_ordinal) {
    for (uint16_t i = 0; i < max_ordinal; i++) {
        if (chunks[i].received) {
            free(chunks[i].data);
            chunks[i].received = 0;
        }
    }
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    unsigned char buffer[MAX_UDP_PAYLOAD];
    int frame_number = 0;
    ChunkEntry chunks[MAX_CHUNKS] = {0};
    uint16_t current_max_ordinal = 0;
    
    ImageBuffer img = {
        .data = malloc(MAX_WIDTH * MAX_HEIGHT * RGB_BYTES_PER_PIXEL),
        .width = MAX_WIDTH,
        .height = MAX_HEIGHT,
        .current_x = 0,
        .current_y = 0
    };

    if (!img.data) {
        perror("Failed to allocate image buffer");
        return 1;
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        free(img.data);
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        free(img.data);
        close(sockfd);
        return 1;
    }

    printf("UDP server listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        ssize_t n = recvfrom(sockfd, buffer, MAX_UDP_PAYLOAD, 0,
                            (struct sockaddr *)&client_addr, &client_len);
        
        if (n < 0) {
            perror("Receive error");
            continue;
        }

        if (n >= 3) {
            uint16_t ordinal = (buffer[0] << 8) | buffer[1];
            
            // Check if this is an end-of-frame marker
            if (buffer[2] == '\t') {
                printf("Received end frame marker (ordinal %d)\n", ordinal);
                current_max_ordinal = ordinal;
                
                // Wait up to 1 second for missing chunks
                time_t start_time = time(NULL);
                int missing_chunks = 0;
                
                do {
                    missing_chunks = 0;
                    for (uint16_t i = 0; i < current_max_ordinal; i++) {
                        if (!chunks[i].received) {
                            missing_chunks++;
                        }
                    }
                    
                    if (missing_chunks > 0 && (time(NULL) - start_time) < 1) {
                        // Quick receive for any pending chunks
                        struct timeval tv = {0, 10000}; // 10ms timeout
                        fd_set fds;
                        FD_ZERO(&fds);
                        FD_SET(sockfd, &fds);
                        
                        if (select(sockfd + 1, &fds, NULL, NULL, &tv) > 0) {
                            n = recvfrom(sockfd, buffer, MAX_UDP_PAYLOAD, 0,
                                       (struct sockaddr *)&client_addr, &client_len);
                            if (n >= 3) {
                                ordinal = (buffer[0] << 8) | buffer[1];
                                if (ordinal < current_max_ordinal && !chunks[ordinal].received) {
                                    chunks[ordinal].data = malloc(n);
                                    memcpy(chunks[ordinal].data, buffer, n);
                                    chunks[ordinal].size = n;
                                    chunks[ordinal].received = 1;
                                    missing_chunks--;
                                }
                            }
                        }
                    }
                } while (missing_chunks > 0 && (time(NULL) - start_time) < 10);

                if (missing_chunks > 0) {
                    printf("Warning: %d chunks still missing after timeout\n", missing_chunks);
                }

                // Process chunks and write frame
                process_chunks_to_image(&img, chunks, current_max_ordinal);
                write_png(&img, frame_number++);
                cleanup_chunks(chunks, current_max_ordinal);
                current_max_ordinal = 0;
                
                // Clear image buffer for next frame
                memset(img.data, 0, MAX_WIDTH * MAX_HEIGHT * RGB_BYTES_PER_PIXEL);
                printf("Done\n");
            }
            else {
                // Regular chunk
                if (ordinal < MAX_CHUNKS) {
                    chunks[ordinal].data = malloc(n);
                    memcpy(chunks[ordinal].data, buffer, n);
                    chunks[ordinal].size = n;
                    chunks[ordinal].received = 1;
                    printf("Received chunk %d (size: %zd)\n", ordinal, n);
                }
            }
        }
    }

    free(img.data);
    cleanup_chunks(chunks, current_max_ordinal);
    close(sockfd);
    return 0;
}