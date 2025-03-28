#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Imlib2.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char *name;
    long long number;  // Using long long for microsecond timestamps
} ImageFile;

int compare(const void *a, const void *b) {
    long long diff = ((ImageFile*)a)->number - ((ImageFile*)b)->number;
    return (diff > 0) - (diff < 0);
}

int main() {
    Display *display;
    Window window;
    Visual *visual;
    Colormap colormap;
    int screen;
    
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    screen = DefaultScreen(display);
    visual = DefaultVisual(display, screen);
    colormap = DefaultColormap(display, screen);
    
    // Changed window size from 192x192 to 384x384
    window = XCreateSimpleWindow(display, RootWindow(display, screen),
                               100, 100, 384, 384, 1,
                               BlackPixel(display, screen),
                               WhitePixel(display, screen));
    
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);
    XFlush(display);
    
    imlib_context_set_display(display);
    imlib_context_set_visual(visual);
    imlib_context_set_colormap(colormap);
    imlib_context_set_drawable(window);
    
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
    
    XEvent event;
    int running = 1;
    
    while (running) {
        for (int i = 0; i < file_count && running; i++) {
            printf("Displaying %s\n", files[i].name);
            
            Imlib_Image img = imlib_load_image(files[i].name);
            if (img) {
                imlib_context_set_image(img);
                // Changed rendering size from 192x192 to 384x384
                imlib_render_image_on_drawable_at_size(0, 0, 384, 384);
                imlib_free_image();
                XFlush(display);
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
            
            while (XPending(display)) {
                XNextEvent(display, &event);
                if (event.type == KeyPress) {
                    running = 0;
                    break;
                }
            }
        }
    }
    
    for (int i = 0; i < file_count; i++) {
        free(files[i].name);
    }
    free(files);
    XCloseDisplay(display);
    return 0;
}