// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

/*
gcc -o a.out standalone.c codec21.c display.c -lX11 -lImlib2 && ./a.out
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <Imlib2.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include "codec21.h"
#include "display.h"

// Increasing it to verify lossless compression quality.
#define TEST_DELAY 1

typedef struct {
    char *name;
    long long number;  // Using long long for microsecond timestamps
} ImageFile;

int running = 1;

// Add this global variable to track when the last frame was displayed
struct timeval last_display_time;

// Add a structure to store compressed data for each line
typedef struct {
    uint8_t* data;    // Compressed data for this line
    size_t size;      // Size of the compressed data
    int line_number;  // Original line number for reference
} CompressedLine;

// Thread worker structure for encoding
typedef struct {
    Vector3D* image_data;
    Vector3D* reference_frame_copy;
    int width;
    int height;
    int start_line;
    int end_line;
    CompressedLine* compressed_lines;
    size_t* total_bytes_compressed;
    size_t* total_compressible_size;
    double* frame_encode_time_us;
    pthread_mutex_t* stats_mutex;  // For thread-safe updates to shared counters
} EncodeThreadArgs;

// Helper function to calculate elapsed time in microseconds between two timevals
long long time_diff_us(struct timeval start, struct timeval end) {
    return ((end.tv_sec - start.tv_sec) * 1000000LL) + 
           (end.tv_usec - start.tv_usec);
}

// Thread worker function for encoding lines
void* encode_thread_worker(void* arg) {
    EncodeThreadArgs* args = (EncodeThreadArgs*)arg;
    size_t thread_bytes_compressed = 0;
    size_t thread_compressible_size = 0;
    double thread_encode_time_us = 0.0;
    
    for (int line = args->start_line; line < args->end_line; line++) {
        // Calculate the starting position for this line
        int start_pos = line * args->width;
        
        // Track the compressible size in bytes
        thread_compressible_size += args->width * sizeof(Vector3D);
        
        // Allocate buffer for this line's compressed data
        uint8_t* temp_buffer = malloc(args->width * sizeof(Vector3D) * 2 + 1);
        if (!temp_buffer) {
            fprintf(stderr, "Failed to allocate buffer for line %d\n", line);
            continue;
        }
        
        // Compress this line
        struct timeval encode_start, encode_end;
        gettimeofday(&encode_start, NULL);
        
        size_t line_compressed_size = encode_block(
            &args->image_data[start_pos],
            &args->reference_frame_copy[start_pos],
            args->width,
            temp_buffer,
            args->width * sizeof(Vector3D) * 2
        );
        
        gettimeofday(&encode_end, NULL);
        long long encode_elapsed_us = time_diff_us(encode_start, encode_end);
        thread_encode_time_us += encode_elapsed_us;
        
        // Store the compressed data for later decoding
        args->compressed_lines[line].data = temp_buffer;
        args->compressed_lines[line].size = line_compressed_size;
        args->compressed_lines[line].line_number = line;
        
        thread_bytes_compressed += line_compressed_size;
    }
    
    // Update shared statistics with mutex protection
    pthread_mutex_lock(args->stats_mutex);
    *args->total_bytes_compressed += thread_bytes_compressed;
    *args->total_compressible_size += thread_compressible_size;
    *args->frame_encode_time_us += thread_encode_time_us;
    pthread_mutex_unlock(args->stats_mutex);
    
    return NULL;
}

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

// Helper function to calculate elapsed time in milliseconds between two timevals
double time_diff_ms(struct timeval start, struct timeval end) {
    return ((end.tv_sec - start.tv_sec) * 1000.0) + 
           ((end.tv_usec - start.tv_usec) / 1000.0);
}

// Modified process_images function with separated encoding and decoding
void *process_images(void *arg) {
    // Initialize the last display time to the start of the program
    gettimeofday(&last_display_time, NULL);

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
            //printf("Loading file %s\n", files[i].name);
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
            
            // Process the same image for 8 frames
            for (int frame = 0; frame < TEST_DELAY && running; frame++) {
                //printf("Processing %s (frame %d of 8)\n", files[i].name, frame + 1);
                
                int width = WIDTH;
                int height = HEIGHT;
                
                // Allocate array to store compressed data for each line
                CompressedLine* compressed_lines = malloc(height * sizeof(CompressedLine));
                Vector3D* reference_frame_copy = malloc(WIDTH * HEIGHT * sizeof(Vector3D));
                
                if (compressed_lines && reference_frame_copy) {
                    size_t total_bytes_compressed = 0;
                    size_t total_bytes_decompressed = 0;
                    size_t total_compressible_size = 0;
                    
                    // Reset timing statistics for this frame
                    double frame_encode_time_us = 0.0;
                    double frame_decode_time_us = 0.0;
                    
                    // Make a copy of the reference frame at the start of frame processing
                    memcpy(reference_frame_copy, reference_frame, WIDTH * HEIGHT * sizeof(Vector3D));
                    
                    // ENCODING PHASE: Encode all lines using four threads
                    struct timeval encode_start_total, encode_end_total;
                    gettimeofday(&encode_start_total, NULL);
                    
                    // Create mutex for thread-safe updates to shared statistics
                    pthread_mutex_t stats_mutex;
                    pthread_mutex_init(&stats_mutex, NULL);
                    
                    // Prepare thread arguments and threads
                    const int num_threads = 20;  // Changed from 10 to 20
                    pthread_t encode_threads[num_threads];
                    EncodeThreadArgs thread_args[num_threads];
                    int lines_per_thread = height / num_threads;
                    
                    for (int t = 0; t < num_threads; t++) {
                        thread_args[t].image_data = image_data;
                        thread_args[t].reference_frame_copy = reference_frame_copy;
                        thread_args[t].width = width;
                        thread_args[t].height = height;
                        thread_args[t].start_line = t * lines_per_thread;
                        thread_args[t].end_line = (t == num_threads - 1) ? height : (t + 1) * lines_per_thread;
                        thread_args[t].compressed_lines = compressed_lines;
                        thread_args[t].total_bytes_compressed = &total_bytes_compressed;
                        thread_args[t].total_compressible_size = &total_compressible_size;
                        thread_args[t].frame_encode_time_us = &frame_encode_time_us;
                        thread_args[t].stats_mutex = &stats_mutex;
                        
                        if (pthread_create(&encode_threads[t], NULL, encode_thread_worker, &thread_args[t]) != 0) {
                            fprintf(stderr, "Failed to create encode thread %d\n", t);
                            // Fall back to single-threaded encoding for this section
                            encode_thread_worker(&thread_args[t]);
                        }
                    }
                    
                    // Wait for all encoding threads to complete
                    for (int t = 0; t < num_threads; t++) {
                        pthread_join(encode_threads[t], NULL);
                    }
                    
                    // Clean up mutex
                    pthread_mutex_destroy(&stats_mutex);
                    
                    gettimeofday(&encode_end_total, NULL);
                    long long encode_wall_time_total_us = time_diff_us(encode_start_total, encode_end_total);
                    
                    printf("  Parallel encoding with %d threads completed\n", num_threads);
                    
                    // DECODING PHASE: Decode all lines in order
                    struct timeval decode_start_total, decode_end_total;
                    gettimeofday(&decode_start_total, NULL);
                    
                    for (int line = 0; line < height; line++) {
                        // Skip lines that failed compression
                        if (!compressed_lines[line].data) continue;
                        
                        int start_pos = line * width;
                        
                        // Decode this line
                        struct timeval decode_start, decode_end;
                        gettimeofday(&decode_start, NULL);
                        
                        size_t line_decompressed_size = decode_blocks(
                            compressed_lines[line].data,
                            compressed_lines[line].size,
                            &reference_frame[start_pos],
                            &reference_frame_copy[start_pos]
                        );
                        
                        gettimeofday(&decode_end, NULL);
                        long long decode_elapsed_us = time_diff_us(decode_start, decode_end);
                        frame_decode_time_us += decode_elapsed_us;
                        
                        total_bytes_decompressed += line_decompressed_size * sizeof(Vector3D);
                    }
                    
                    gettimeofday(&decode_end_total, NULL);
                    long long decode_total_us = time_diff_us(decode_start_total, decode_end_total);
                    
                    // Free all compressed line buffers
                    for (int line = 0; line < height; line++) {
                        if (compressed_lines[line].data) {
                            free(compressed_lines[line].data);
                        }
                    }
                    
                    // Print statistics
                    double compression_ratio = (double)total_compressible_size / (double)total_bytes_compressed;
                    printf("Frame statistics:\n");
                    printf("  Compressed size: %zu bytes\n", total_bytes_compressed);
                    printf("  Decompressed size: %zu bytes\n", total_bytes_decompressed);
                    printf("  Compressible size: %zu bytes\n", total_compressible_size);
                    printf("  Compression ratio: %.2f:1\n", compression_ratio);
                    
                    if (total_compressible_size != total_bytes_decompressed) {
                        fprintf(stderr, "ERROR: Size mismatch! Compression is not lossless!\n");
                        exit(1);
                    }
                    
                    // Timing for display_frame
                    struct timeval display_start, display_end;
                    gettimeofday(&display_start, NULL);
                    
                    // Display the frame after it's been fully decoded
                    display_frame(reference_frame);
                    
                    gettimeofday(&display_end, NULL);
                    long long display_elapsed_us = time_diff_us(display_start, display_end);
                    
                    // Print timing information for this frame
                    printf("Timing statistics:\n");
                    printf("  Encode processor time total: %.2f µs\n", frame_encode_time_us);
                    printf("  Encode wall time total: %.2f µs\n", (double)encode_wall_time_total_us);
                    printf("  Decode total: %.2f µs\n", (double)decode_total_us);
                    printf("  Display time: %.2f µs\n", (double)display_elapsed_us);
                    printf("  Total processing time: %.2f µs\n", 
                           (double)(encode_wall_time_total_us + decode_total_us + display_elapsed_us));
                    
                    // Record the time immediately after displaying this frame
                    struct timeval current_time;
                    gettimeofday(&current_time, NULL);
                    last_display_time = current_time;
                    
                    // Free allocated resources
                    free(compressed_lines);
                    free(reference_frame_copy);
                } else {
                    if (compressed_lines) free(compressed_lines);
                    if (reference_frame_copy) free(reference_frame_copy);
                    fprintf(stderr, "Failed to allocate buffers for encoding\n");
                }
                
                if (TEST_DELAY) {
                    // 30ms delay between frames of the same image
                    usleep(30000);
                }
            }
            
            // Free the image data after all frames are processed
            free(image_data);
            
            if (i < file_count - 1) {
                // Calculate time since last frame was displayed
                struct timeval current_time;
                gettimeofday(&current_time, NULL);
                
                long long elapsed_us = (current_time.tv_sec - last_display_time.tv_sec) * 1000000 +
                                      (current_time.tv_usec - last_display_time.tv_usec);
                
                // Add delay between different images, adjusted by elapsed time
                long long target_delay_us = files[i + 1].number - files[i].number;
                if (target_delay_us > elapsed_us) {
                    printf("Delaying before next file: %lld microseconds (adjusted to %lld)\n", 
                           target_delay_us, target_delay_us - elapsed_us);
                    usleep((useconds_t)(target_delay_us - elapsed_us));
                }
            } else {
                // When we reach the last file, small delay before looping back to first file
                struct timeval current_time;
                gettimeofday(&current_time, NULL);
                
                long long elapsed_us = (current_time.tv_sec - last_display_time.tv_sec) * 1000000 +
                                      (current_time.tv_usec - last_display_time.tv_usec);
                
                long long target_delay_us = 30000;  // 30ms delay before restarting
                if (target_delay_us > elapsed_us) {
                    printf("Reached last file, looping back to beginning (delay: %lld us)\n", 
                           target_delay_us - elapsed_us);
                    usleep((useconds_t)(target_delay_us - elapsed_us));
                }
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
    // Initialize display
    if (init_display() == 0) {
        fprintf(stderr, "Failed to initialize display\n");
        return 1;
    }
    printf("Display initialized successfully\n");
    
    // Create thread for processing images
    pthread_t processing_thread;
    if (pthread_create(&processing_thread, NULL, process_images, NULL) != 0) {
        perror("Failed to create processing thread");
        running = 0;
        return 1;
    }
    
    printf("Image processing started\n");
    
    // Wait for thread to finish (it only exits on program termination)
    pthread_join(processing_thread, NULL);
    
    // Clean up display resources
    cleanup_display();
    printf("Display resources cleaned up\n");
    
    return 0;
}