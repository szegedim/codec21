// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <Imlib2.h>
#include "codec21.h"
#include "display.h"

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 640

static Display *display = NULL;
static Window window;
static Visual *visual;
static Colormap colormap;
static Imlib_Image buffer_image = NULL;
static DATA32 *image_data = NULL;

int init_display(void) {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        return 0;
    }
    
    int screen = DefaultScreen(display);
    visual = DefaultVisual(display, screen);
    colormap = DefaultColormap(display, screen);
    
    window = XCreateSimpleWindow(display, RootWindow(display, screen),
                               100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, 1,
                               BlackPixel(display, screen),
                               BlackPixel(display, screen));
    
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);
    XFlush(display);
    
    imlib_context_set_display(display);
    imlib_context_set_visual(visual);
    imlib_context_set_colormap(colormap);
    imlib_context_set_drawable(window);
    
    // Create an Imlib2 image to work with
    buffer_image = imlib_create_image(WIDTH, HEIGHT);
    if (!buffer_image) {
        fprintf(stderr, "Failed to create Imlib2 image\n");
        return 0;
    }
    
    imlib_context_set_image(buffer_image);
    image_data = imlib_image_get_data();
    if (!image_data) {
        fprintf(stderr, "Failed to get image data\n");
        imlib_free_image();
        return 0;
    }
    
    // Clear the image initially
    memset(image_data, 0, WIDTH * HEIGHT * 4);
    
    printf("Display initialized successfully with window size %dx%d\n", 
           WINDOW_WIDTH, WINDOW_HEIGHT);
    return 1;
}

void display_frame(const Vector3D* buffer) {
    if (!display || !buffer_image || !buffer || !image_data) return;
    
    imlib_context_set_image(buffer_image);
    
    // Convert Vector3D buffer to Imlib2 image format (ARGB)
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            const Vector3D* pixel = &buffer[y * WIDTH + x];
            int idx = y * WIDTH + x;
            
            // ARGB format for Imlib2
            image_data[idx] = (0xFF << 24) | // Alpha
                             ((pixel->x & 0xFF) << 16) | // Red
                             ((pixel->y & 0xFF) << 8) | // Green
                             (pixel->z & 0xFF); // Blue
        }
    }
    
    imlib_image_set_has_alpha(1);
    
    // Render the image scaled to the window size
    imlib_render_image_on_drawable_at_size(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    XFlush(display);
}

void cleanup_display(void) {
    if (buffer_image) {
        imlib_context_set_image(buffer_image);
        imlib_free_image();
    }
    
    if (display) {
        XCloseDisplay(display);
        display = NULL;
    }
}