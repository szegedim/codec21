/*
export PIPE_PATH="https://www.theme25.com/$(uuidgen | sha256sum | cut -d' ' -f1).tig?Content-Type=text/plain"; export STDIN_PATH="https://www.theme25.com/$(uuidgen | sha256sum | cut -d' ' -f1).tig?append=1"; export STDOUT_PATH="https://www.theme25.com/$(uuidgen | sha256sum | cut -d' ' -f1).tig?Content-Type=image/png"; printf "$STDIN_PATH\n$STDOUT_PATH\n" | curl -X PUT --data-binary @- "$PIPE_PATH"; echo; echo $PIPE_PATH; gcc script_to_png.c -o script_to_png.out -I/usr/include/freetype2 -lpng -lfreetype -lcurl -lpthread -lz -lm && (./my_script.sh | ./script_to_png.out "$STDOUT_PATH")

 script_to_png.c -o script_to_png.out -I/usr/include/freetype2 -lpng -lfreetype -lcurl -lpthread -lz -lm && (./my_script.sh | ./script_to_png.out https://www.theme25.com/$(uuidgen | sha256sum | cut -d' ' -f1).tig?Content-Type=image/png)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <curl/curl.h>
#include <freetype2/ft2build.h>

#include FT_FREETYPE_H
#include <png.h>

// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

#define MAX_OUTPUT_LINES 180
#define LINE_BUFFER_SIZE 1024
#define WIDTH 640
#define HEIGHT 800
#define FONT_SIZE 12

// Global flag to control program execution
volatile bool running = true;

typedef struct {
    unsigned char* buffer;
    int width;
    int height;
} Image;

// Structure to hold PNG data in memory
typedef struct {
    unsigned char *buffer;
    size_t size;
} MemoryStruct;

// Structure for libcurl read callback
typedef struct {
    const unsigned char *data;
    size_t size;
    size_t pos;
} UploadData;


// Thread data structure for PNG generation/upload
typedef struct {
    FT_Face face;
    const char* upload_url; // <-- Changed from output_dir
    char (*lines)[LINE_BUFFER_SIZE];
    int* line_count;
    pthread_mutex_t* mutex;
} PngThreadData;

// Thread data structure for script execution
typedef struct {
    const char* script_path;
    char (*lines)[LINE_BUFFER_SIZE];
    int* line_count;
    pthread_mutex_t* mutex;
} ScriptThreadData;

// Signal handler to gracefully exit the program
void handle_signal(int sig) {
    printf("\nReceived signal %d, stopping image generation...\n", sig);
    running = false;
}

// Initialize a blank image with white background
Image* create_image(int width, int height) {
    Image* img = (Image*)malloc(sizeof(Image));
    if (!img) {
        fprintf(stderr, "Failed to allocate memory for image structure\n");
        return NULL;
    }

    img->width = width;
    img->height = height;
    // Use calloc for zero-initialization (though we overwrite below)
    img->buffer = (unsigned char*)calloc(width * height * 4, sizeof(unsigned char));

    if (!img->buffer) {
        fprintf(stderr, "Failed to allocate memory for image buffer\n");
        free(img);
        return NULL;
    }

    // Set background to white (RGBA)
    for (int i = 0; i < width * height * 4; i += 4) {
        img->buffer[i] = 255;
        img->buffer[i + 1] = 255;
        img->buffer[i + 2] = 255;
        img->buffer[i + 3] = 255;
    }

    return img;
}

// Draw text on image using FreeType (Unchanged)
void draw_text(Image* img, FT_Face face, char* text, int x, int y) {
    if (!img || !img->buffer || !text) return;

    FT_Error error;
    int pen_x = x;
    int pen_y = y;

    for (int i = 0; i < strlen(text); i++) {
        error = FT_Load_Char(face, text[i], FT_LOAD_RENDER);
        if (error)
            continue;

        FT_Bitmap* bitmap = &face->glyph->bitmap;

        for (int row = 0; row < bitmap->rows; row++) {
            for (int col = 0; col < bitmap->width; col++) {
                int x_pos = pen_x + face->glyph->bitmap_left + col;
                int y_pos = pen_y - face->glyph->bitmap_top + row;

                if (x_pos < 0 || y_pos < 0 || x_pos >= img->width || y_pos >= img->height)
                    continue;

                unsigned char pixel = bitmap->buffer[row * bitmap->pitch + col];
                if (pixel > 0) { // Use alpha value for blending (black text)
                    int index = (y_pos * img->width + x_pos) * 4;
                    // Basic alpha blending (assuming white background)
                    float alpha = pixel / 255.0f;
                    img->buffer[index] = (unsigned char)(img->buffer[index] * (1.0f - alpha)); // R=0
                    img->buffer[index + 1] = (unsigned char)(img->buffer[index+1] * (1.0f - alpha)); // G=0
                    img->buffer[index + 2] = (unsigned char)(img->buffer[index+2] * (1.0f - alpha)); // B=0
                    // Alpha remains 255 (fully opaque pixel content)
                }
            }
        }
        pen_x += face->glyph->advance.x >> 6;
    }
}


// Custom write function for libpng to write to memory
static void png_memory_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    MemoryStruct* mem = (MemoryStruct*)png_get_io_ptr(png_ptr);
    size_t new_size = mem->size + length;

    // Reallocate memory buffer
    unsigned char* new_buffer = realloc(mem->buffer, new_size);
    if (!new_buffer) {
        png_error(png_ptr, "Memory allocation error in png_memory_write_data");
        return;
    }
    mem->buffer = new_buffer;
    memcpy(mem->buffer + mem->size, data, length);
    mem->size += length;
}

// Custom flush function for libpng (can be empty for memory)
static void png_memory_flush_data(png_structp png_ptr) {
    // Nothing to do
}

// Read callback function for libcurl
static size_t png_read_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    UploadData *upload_ctx = (UploadData *)userdata;
    size_t buffer_size = size * nitems;
    size_t bytes_to_copy = 0;

    if (upload_ctx->pos < upload_ctx->size) {
        bytes_to_copy = upload_ctx->size - upload_ctx->pos;
        if (bytes_to_copy > buffer_size) {
            bytes_to_copy = buffer_size;
        }
        memcpy(buffer, upload_ctx->data + upload_ctx->pos, bytes_to_copy);
        upload_ctx->pos += bytes_to_copy;
        return bytes_to_copy;
    }

    return 0; // No more data
}


// Encode image to PNG in memory and upload via HTTP PUT
int upload_png_http(Image* img, const char* url) {
    if (!img || !img->buffer || !url) {
        fprintf(stderr, "Invalid parameters for upload_png_http\n");
        return -1;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Failed to create PNG write structure\n");
        return -1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "Failed to create PNG info structure\n");
        png_destroy_write_struct(&png, NULL);
        return -1;
    }

    // Initialize memory structure for PNG data
    MemoryStruct png_mem = { .buffer = NULL, .size = 0 };
    png_mem.buffer = malloc(1024); // Initial allocation
    if (!png_mem.buffer) {
         fprintf(stderr, "Failed initial memory allocation for PNG buffer\n");
         png_destroy_write_struct(&png, &info);
         return -1;
    }
    png_mem.size = 0; // Start with size 0, will grow

    // Set error handling
    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Error during PNG creation\n");
        png_destroy_write_struct(&png, &info);
        free(png_mem.buffer);
        return -1;
    }

    // Set custom write functions to write to memory
    png_set_write_fn(png, &png_mem, png_memory_write_data, png_memory_flush_data);

    // Set PNG header info
    png_set_IHDR(png, info, img->width, img->height, 8,
                 PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png, info);

    // Prepare row pointers
    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * img->height);
    if (!row_pointers) {
        fprintf(stderr, "Failed to allocate memory for PNG row pointers\n");
        png_destroy_write_struct(&png, &info);
        free(png_mem.buffer);
        return -1;
    }
    for (int y = 0; y < img->height; y++) {
        row_pointers[y] = img->buffer + y * img->width * 4;
    }

    // Write image data to memory buffer
    png_write_image(png, row_pointers);
    png_write_end(png, NULL);

    // Clean up row pointers and libpng structures
    free(row_pointers);
    png_destroy_write_struct(&png, &info);

    // Now png_mem.buffer contains the PNG data, and png_mem.size is its size

    // --- Use libcurl to upload the PNG data ---
    CURL *curl = NULL;
    CURLcode res = CURLE_FAILED_INIT;
    struct curl_slist *headers = NULL;

    curl = curl_easy_init();
    if (curl) {
        // Prepare data for upload callback
        UploadData upload_ctx = { .data = png_mem.buffer, .size = png_mem.size, .pos = 0 };

        // Set Content-Type header
        headers = curl_slist_append(headers, "Content-Type: image/png");
        if (!headers) {
            fprintf(stderr, "Failed to create curl slist for headers\n");
            curl_easy_cleanup(curl);
            free(png_mem.buffer);
            return -1;
        }

        // Set libcurl options for PUT request
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, png_read_callback);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)png_mem.size);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            printf("Successfully uploaded PNG (%zu bytes) to %s. HTTP Response Code: %ld\n",
                   png_mem.size, url, response_code);
            if (response_code >= 400) {
                 fprintf(stderr, "Warning: Server responded with error code %ld\n", response_code);
                 // Consider treating server errors as upload failures
                 res = CURLE_HTTP_RETURNED_ERROR; // Set an error status
            }
        }

        // Clean up libcurl
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } else {
        fprintf(stderr, "Failed to initialize curl easy handle\n");
    }

    // Free the memory buffer holding the PNG data
    free(png_mem.buffer);

    return (res == CURLE_OK) ? 0 : -1; // Return 0 on success, -1 on failure
}


// Execute script and capture output (Modified to use circular buffer behavior)
void execute_script(ScriptThreadData* data, const char* script_path, char lines[][LINE_BUFFER_SIZE], int* line_count) {
     *line_count = 0;

     char command[256];
     snprintf(command, sizeof(command), "bash %s", script_path);

     FILE* pipe = popen(command, "r");
     if (!pipe) {
         fprintf(stderr, "Failed to execute script: %s\n", script_path);
         return;
     }

     char line[LINE_BUFFER_SIZE];
     while (fgets(line, LINE_BUFFER_SIZE, pipe) != NULL) {
        pthread_mutex_lock(data->mutex);
        
        // If we've reached MAX_OUTPUT_LINES, shift all lines up by one
        if (*line_count >= MAX_OUTPUT_LINES) {
            // Shift all lines up (discard the oldest)
            for (int i = 0; i < MAX_OUTPUT_LINES - 1; i++) {
                strncpy(lines[i], lines[i + 1], LINE_BUFFER_SIZE - 1);
                lines[i][LINE_BUFFER_SIZE - 1] = '\0';
            }
            // New line goes at the end (but don't increment line_count anymore)
            strncpy(lines[MAX_OUTPUT_LINES - 1], line, LINE_BUFFER_SIZE - 1);
            lines[MAX_OUTPUT_LINES - 1][LINE_BUFFER_SIZE - 1] = '\0';
        } else {
            // If we haven't reached the maximum, just add to the end and increment
            strncpy(lines[*line_count], line, LINE_BUFFER_SIZE - 1);
            lines[*line_count][LINE_BUFFER_SIZE - 1] = '\0';
            (*line_count)++;
        }
        
        // Remove trailing newline (for both cases)
        size_t len = strlen(lines[(*line_count > 0) ? *line_count - 1 : MAX_OUTPUT_LINES - 1]);
        if (len > 0 && lines[(*line_count > 0) ? *line_count - 1 : MAX_OUTPUT_LINES - 1][len-1] == '\n') {
            lines[(*line_count > 0) ? *line_count - 1 : MAX_OUTPUT_LINES - 1][len-1] = '\0';
        }
        
        pthread_mutex_unlock(data->mutex);
    }

    pclose(pipe);
}

// Get current time in microseconds (Unchanged)
long long get_microseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
}

// Thread function to execute script (Unchanged)
void* script_thread0(void* arg) {
    ScriptThreadData* data = (ScriptThreadData*)arg;

    execute_script(data, data->script_path, data->lines, data->line_count);

    return NULL;
}

// Thread function to generate and upload PNGs
void* generate_and_upload_pngs(void* arg) {
    PngThreadData* data = (PngThreadData*)arg;
    FT_Face face = data->face;
    const char* upload_url = data->upload_url; // Use the URL
    char (*lines)[LINE_BUFFER_SIZE] = data->lines;
    int* line_count = data->line_count;
    pthread_mutex_t* mutex = data->mutex;

    long long start_time = get_microseconds();
    int frame_count = 0;

    while (running) {
        long long current_time = get_microseconds();
        long long elapsed_us = current_time - start_time;
        // Aim for roughly 1 frame per second (adjust as needed)
        long long expected_frame_time = frame_count * 1000000LL; // 1 second intervals

        if (elapsed_us >= expected_frame_time) {
            frame_count++;

            // Create image
            Image* img = create_image(WIDTH, HEIGHT);
            if (!img) {
                fprintf(stderr, "Failed to create image for frame %d\n", frame_count);
                // Avoid busy-waiting on failure
                usleep(100000); // Sleep 100ms before retrying
                continue;
            }

            // Add timestamp
            char timestamp[128];
            snprintf(timestamp, sizeof(timestamp), "Frame %d - Elapsed: %.3f s",
                     frame_count, elapsed_us / 1000000.0);
            draw_text(img, face, timestamp, 10, 20);

            // Draw script output or placeholder
            pthread_mutex_lock(mutex);
            if (*line_count == 0) {
                draw_text(img, face, "Waiting for script output...", 10, 50);
            } else {
                int max_lines_to_draw = (HEIGHT - 60) / (FONT_SIZE + 4); // Calculate based on available space
                int start_line = 0;
                
                // If we have more lines than can fit, start from later in the buffer
                if (*line_count > max_lines_to_draw) {
                    start_line = *line_count - max_lines_to_draw;
                }
                
                // Draw only the most recent lines that fit
                for (int i = 0; i < max_lines_to_draw && (start_line + i) < *line_count; i++) {
                    draw_text(img, face, lines[start_line + i], 10, 50 + i * (FONT_SIZE + 4));
                }
            }
            pthread_mutex_unlock(mutex);

            // --- Upload image via HTTP PUT ---
            int upload_result = upload_png_http(img, upload_url);
            if (upload_result == 0) {
                printf("Frame %d: Uploaded image at elapsed time %lld us\n",
                       frame_count, elapsed_us);
            } else {
                fprintf(stderr, "Frame %d: Failed to upload image\n", frame_count);
                // Consider adding retry logic or specific error handling here
            }

            // Clean up image resources for this frame
            free(img->buffer);
            free(img);
        }

        // Sleep to avoid busy-waiting and control frame rate
        usleep(10000); // Check roughly every 10ms
    }

    return NULL;
}

// Modified to read from standard input instead of executing script
void* stdin_reader_thread(void* arg) {
    ScriptThreadData* data = (ScriptThreadData*)arg;
    char (*lines)[LINE_BUFFER_SIZE] = data->lines;
    int* line_count = data->line_count;
    pthread_mutex_t* mutex = data->mutex;
    
    char line[LINE_BUFFER_SIZE];
    
    // Read from stdin until EOF
    while (running && fgets(line, LINE_BUFFER_SIZE, stdin) != NULL) {
        pthread_mutex_lock(mutex);
        
        // If we've reached MAX_OUTPUT_LINES, shift all lines up by one
        if (*line_count >= MAX_OUTPUT_LINES) {
            // Shift all lines up (discard the oldest)
            for (int i = 0; i < MAX_OUTPUT_LINES - 1; i++) {
                strncpy(lines[i], lines[i + 1], LINE_BUFFER_SIZE - 1);
                lines[i][LINE_BUFFER_SIZE - 1] = '\0';
            }
            // New line goes at the end (but don't increment line_count anymore)
            strncpy(lines[MAX_OUTPUT_LINES - 1], line, LINE_BUFFER_SIZE - 1);
            lines[MAX_OUTPUT_LINES - 1][LINE_BUFFER_SIZE - 1] = '\0';
        } else {
            // If we haven't reached the maximum, just add to the end and increment
            strncpy(lines[*line_count], line, LINE_BUFFER_SIZE - 1);
            lines[*line_count][LINE_BUFFER_SIZE - 1] = '\0';
            (*line_count)++;
        }
        
        // Remove trailing newline
        size_t len = strlen(lines[(*line_count > 0) ? *line_count - 1 : MAX_OUTPUT_LINES - 1]);
        if (len > 0 && lines[(*line_count > 0) ? *line_count - 1 : MAX_OUTPUT_LINES - 1][len-1] == '\n') {
            lines[(*line_count > 0) ? *line_count - 1 : MAX_OUTPUT_LINES - 1][len-1] = '\0';
        }
        
        pthread_mutex_unlock(mutex);
    }
    
    return NULL;
}

// Modified main function to use stdin reader thread
int main(int argc, char** argv) {
    // --- Updated Usage ---
    if (argc < 2) {
        printf("Usage: %s <upload_url> [font_path]\n", argv[0]);
        printf("Example: %s http://example.com/upload/image.png\n", argv[0]);
        printf("The program will read from standard input and encode the text into PNG images.\n");
        return 1;
    }

    // First argument is now the upload URL
    const char* upload_url = argv[1];
    const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"; // Default font

    if (argc >= 3) {
        font_path = argv[2];
    }

    // Check if font file exists
    if (access(font_path, R_OK) != 0) {
        fprintf(stderr, "Error: Cannot access font file %s: %s\n", font_path, strerror(errno));
        return 1;
    }

    // --- Initialize libcurl ---
    CURLcode global_res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (global_res != CURLE_OK) {
        fprintf(stderr, "Failed to initialize libcurl: %s\n", curl_easy_strerror(global_res));
        return 1;
    }

    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Initialize FreeType
    FT_Library library;
    FT_Face face;
    FT_Error error;

    error = FT_Init_FreeType(&library);
    if (error) {
        fprintf(stderr, "Failed to initialize FreeType library\n");
        curl_global_cleanup(); // Clean up curl
        return 1;
    }

    error = FT_New_Face(library, font_path, 0, &face);
    if (error) {
        fprintf(stderr, "Failed to load font: %s\n", font_path);
        FT_Done_FreeType(library);
        curl_global_cleanup();
        return 1;
    }

    error = FT_Set_Pixel_Sizes(face, 0, FONT_SIZE);
    if (error) {
        fprintf(stderr, "Failed to set font size\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        curl_global_cleanup();
        return 1;
    }

    // Initialize lines array and synchronization primitives
    char lines[MAX_OUTPUT_LINES][LINE_BUFFER_SIZE];
    int line_count = 0;
    pthread_mutex_t mutex;

    pthread_mutex_init(&mutex, NULL);

    // Set up PNG upload thread data
    PngThreadData png_data = {
        .face = face,
        .upload_url = upload_url,
        .lines = lines,
        .line_count = &line_count,
        .mutex = &mutex
    };

    // Start PNG generation/upload thread
    pthread_t png_thread;
    if (pthread_create(&png_thread, NULL, generate_and_upload_pngs, &png_data) != 0) {
        fprintf(stderr, "Failed to create PNG upload thread\n");
        pthread_mutex_destroy(&mutex);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        curl_global_cleanup();
        return 1;
    }

    // Set up stdin reader thread data
    ScriptThreadData stdin_data = {
        .script_path = NULL, // Not needed for stdin
        .lines = lines,
        .line_count = &line_count,
        .mutex = &mutex
    };

    // Start stdin reader thread
    pthread_t stdin_thread;
    if (pthread_create(&stdin_thread, NULL, stdin_reader_thread, &stdin_data) != 0) {
        fprintf(stderr, "Failed to create stdin reader thread\n");
        running = false; // Signal PNG thread to exit
        pthread_join(png_thread, NULL);
        pthread_mutex_destroy(&mutex);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        curl_global_cleanup();
        return 1;
    }

    // Wait for both threads to finish
    pthread_join(png_thread, NULL);
    pthread_join(stdin_thread, NULL);

    // Clean up
    pthread_mutex_destroy(&mutex);
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    curl_global_cleanup();

    printf("Image generation and upload terminated.\n");

    return 0;
}