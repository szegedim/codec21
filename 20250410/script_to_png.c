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

// Try alternative include paths for FreeType
#ifdef __has_include
#  if __has_include(<ft2build.h>)
#    include <ft2build.h>
#  elif __has_include(<freetype2/ft2build.h>)
#    include <freetype2/ft2build.h>
#  else
#    error "Could not find ft2build.h - please install freetype development packages"
#  endif
#else
#  include <freetype2/ft2build.h>
#endif

#include FT_FREETYPE_H
#include <png.h>

#define MAX_OUTPUT_LINES 80
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

// Thread data structure for PNG generation
typedef struct {
    FT_Face face;
    const char* output_dir;
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

// Make sure directory exists
int ensure_directory(const char* dir_path) {
    struct stat st;
    if (stat(dir_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        return -1;
    }

    if (mkdir(dir_path, 0777) != 0) {
        fprintf(stderr, "Error creating directory %s: %s\n", dir_path, strerror(errno));
        return -1;
    }

    return 0;
}

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
    img->buffer = (unsigned char*)calloc(width * height * 4, sizeof(unsigned char));

    if (!img->buffer) {
        fprintf(stderr, "Failed to allocate memory for image buffer\n");
        free(img);
        return NULL;
    }

    for (int i = 0; i < width * height * 4; i += 4) {
        img->buffer[i] = 255;     // R
        img->buffer[i + 1] = 255; // G
        img->buffer[i + 2] = 255; // B
        img->buffer[i + 3] = 255; // A
    }

    return img;
}

// Draw text on image using FreeType
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
                if (pixel > 0) {
                    int index = (y_pos * img->width + x_pos) * 4;
                    img->buffer[index] = 0;
                    img->buffer[index + 1] = 0;
                    img->buffer[index + 2] = 0;
                }
            }
        }

        pen_x += face->glyph->advance.x >> 6;
    }
}

// Save image to PNG file
int save_png(Image* img, const char* filename) {
    if (!img || !img->buffer || !filename) {
        fprintf(stderr, "Invalid parameters for save_png\n");
        return -1;
    }

    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open file for writing: %s (Error: %s)\n",
                filename, strerror(errno));
        return -1;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fprintf(stderr, "Failed to create PNG write structure\n");
        fclose(fp);
        return -1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "Failed to create PNG info structure\n");
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        return -1;
    }

    if (setjmp(png_jmpbuf(png))) {
        fprintf(stderr, "Error during PNG creation\n");
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return -1;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, img->width, img->height, 8,
                PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png, info);

    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * img->height);
    if (!row_pointers) {
        fprintf(stderr, "Failed to allocate memory for PNG row pointers\n");
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return -1;
    }

    for (int y = 0; y < img->height; y++) {
        row_pointers[y] = img->buffer + y * img->width * 4;
    }

    png_write_image(png, row_pointers);
    png_write_end(png, NULL);

    free(row_pointers);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    printf("Successfully wrote image to: %s\n", filename);
    return 0;
}

// Execute script and capture output
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
    while (*line_count < MAX_OUTPUT_LINES &&
           fgets(line, LINE_BUFFER_SIZE, pipe) != NULL) {
        pthread_mutex_lock(data->mutex);
        lines[*line_count][0] = '\0';
        strncpy(lines[*line_count], line, LINE_BUFFER_SIZE);
        size_t len = strlen(lines[*line_count]);
        if (len > 0 && lines[*line_count][len-1] == '\n') {
            lines[*line_count][len-1] = '\0';
        }
        (*line_count)++;
        pthread_mutex_unlock(data->mutex);
    }

    pclose(pipe);
}

// Get current time in microseconds
long long get_microseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
}

// Thread function to execute script
void* script_thread0(void* arg) {
    ScriptThreadData* data = (ScriptThreadData*)arg;

    execute_script(data, data->script_path, data->lines, data->line_count);

    return NULL;
}

// Thread function to generate PNGs
void* generate_pngs(void* arg) {
    PngThreadData* data = (PngThreadData*)arg;
    FT_Face face = data->face;
    const char* output_dir = data->output_dir;
    char (*lines)[LINE_BUFFER_SIZE] = data->lines;
    int* line_count = data->line_count;
    pthread_mutex_t* mutex = data->mutex;

    long long start_time = get_microseconds();
    int frame_count = 0;

    while (running) {
        long long current_time = get_microseconds();
        long long elapsed_us = current_time - start_time;
        long long expected_frame_time = frame_count * 1000000LL;

        if (elapsed_us >= expected_frame_time) {
            frame_count++;

            // Create output filename
            char output_path[512];
            snprintf(output_path, sizeof(output_path), "%s/output_%06d_%lld_us.png",
                    output_dir, frame_count, elapsed_us);

            // Create image
            Image* img = create_image(WIDTH, HEIGHT);
            if (!img) {
                fprintf(stderr, "Failed to create image\n");
                continue;
            }

            // Add timestamp
            char timestamp[64];
            snprintf(timestamp, sizeof(timestamp), "Frame %d - Elapsed: %lld microseconds",
                    frame_count, elapsed_us);
            draw_text(img, face, timestamp, 10, 20);

            // Draw script output or placeholder
            pthread_mutex_lock(mutex);
            if (*line_count == 0) {
                draw_text(img, face, "Waiting for script output...", 10, 50);
            } else {
                for (int i = 0; i < *line_count; i++) {
                    draw_text(img, face, lines[i], 10, 50 + i * (FONT_SIZE + 4));
                }
            }
            pthread_mutex_unlock(mutex);

            // Save image
            int save_result = save_png(img, output_path);
            if (save_result == 0) {
                printf("Frame %d: Saved image at elapsed time %lld us\n",
                       frame_count, elapsed_us);
            } else {
                fprintf(stderr, "Frame %d: Failed to save image\n", frame_count);
            }

            free(img->buffer);
            free(img);
        }

        usleep(10000); // 10ms
    }

    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <bash_script_path> <output_dir> [font_path]\n", argv[0]);
        return 1;
    }

    const char* script_path = argv[1];
    const char* output_dir = argv[2];
    const char* font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

    if (argc >= 4) {
        font_path = argv[3];
    }

    // Make sure the output directory exists
    if (ensure_directory(output_dir) != 0) {
        fprintf(stderr, "Error: Output directory issue with %s\n", output_dir);
        return 1;
    }

    // Check write permissions
    if (access(output_dir, W_OK) != 0) {
        fprintf(stderr, "Error: No write permission for %s\n", output_dir);
        return 1;
    }

    // Check if script exists
    FILE* test_script = fopen(script_path, "r");
    if (!test_script) {
        fprintf(stderr, "Error: Cannot open script file %s: %s\n",
                script_path, strerror(errno));
        return 1;
    }
    fclose(test_script);

    // Check if font file exists
    if (access(font_path, R_OK) != 0) {
        fprintf(stderr, "Error: Cannot access font file %s\n", font_path);
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
        return 1;
    }

    error = FT_New_Face(library, font_path, 0, &face);
    if (error) {
        fprintf(stderr, "Failed to load font: %s\n", font_path);
        FT_Done_FreeType(library);
        return 1;
    }

    error = FT_Set_Pixel_Sizes(face, 0, FONT_SIZE);
    if (error) {
        fprintf(stderr, "Failed to set font size\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    // Initialize lines array and synchronization primitives
    char lines[MAX_OUTPUT_LINES][LINE_BUFFER_SIZE];
    int line_count = 0;
    pthread_mutex_t mutex;

    pthread_mutex_init(&mutex, NULL);

    // Set up PNG thread data
    PngThreadData png_data = {
        .face = face,
        .output_dir = output_dir,
        .lines = lines,
        .line_count = &line_count,
        .mutex = &mutex
    };

    // Start PNG generation thread
    pthread_t png_thread;
    if (pthread_create(&png_thread, NULL, generate_pngs, &png_data) != 0) {
        fprintf(stderr, "Failed to create PNG generation thread\n");
        pthread_mutex_destroy(&mutex);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    // Set up script thread data
    ScriptThreadData script_data = {
        .script_path = script_path,
        .lines = lines,
        .line_count = &line_count,
        .mutex = &mutex
    };

    // Start script execution thread
    pthread_t script_thread;
    if (pthread_create(&script_thread, NULL, script_thread0, &script_data) != 0) {
        fprintf(stderr, "Failed to create script execution thread\n");
        running = false; // Signal PNG thread to exit
        pthread_join(png_thread, NULL);
        pthread_mutex_destroy(&mutex);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return 1;
    }

    // Wait for both threads to finish
    pthread_join(png_thread, NULL);
    pthread_join(script_thread, NULL);

    // Clean up
    pthread_mutex_destroy(&mutex);
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    printf("Image generation terminated.\n");

    return 0;
}
