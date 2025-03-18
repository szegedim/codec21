// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

// gcc receiver.c codec21.c display.c -lX11 -lImlib2

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "codec21.h"
#include "display.h"

static Display *display = NULL;
static Window window;
static GC gc;
static XImage *image = NULL;
static char *image_data = NULL;

int init_display(void) {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        return 0;
    }
    
    int screen = DefaultScreen(display);
    window = XCreateSimpleWindow(display, RootWindow(display, screen),
                               0, 0, WIDTH, HEIGHT, 0,
                               BlackPixel(display, screen),
                               BlackPixel(display, screen));
    
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);
    
    gc = XCreateGC(display, window, 0, NULL);
    
    // Create XImage
    Visual *visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);
    
    image_data = (char*)malloc(WIDTH * HEIGHT * 4); // 32-bit aligned
    if (!image_data) return 0;
    
    image = XCreateImage(display, visual, depth, ZPixmap, 0,
                        image_data, WIDTH, HEIGHT, 32, 0);
    if (!image) {
        free(image_data);
        return 0;
    }
    
    return 1;
}

void display_frame(const Vector3D* buffer) {
    if (!display || !image || !buffer) return;
    
    // Convert Vector3D buffer to XImage format
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            const Vector3D* pixel = &buffer[y * WIDTH + x];
            int idx = (y * WIDTH + x) * 4;
            
            image_data[idx + 0] = pixel->z;  // Blue
            image_data[idx + 1] = pixel->y;  // Green
            image_data[idx + 2] = pixel->x;  // Red
            image_data[idx + 3] = 0xFF;      // Alpha
        }
    }
    
    // Display the image
    XPutImage(display, window, gc, image,
              0, 0, 0, 0, WIDTH, HEIGHT);
    XFlush(display);
    
    // Set kanban to indicate frame is displayed
    kanban = 1;
}

void cleanup_display(void) {
    if (image) {
        image->data = NULL;  // Prevent XDestroyImage from freeing our buffer
        XDestroyImage(image);
    }
    if (image_data) free(image_data);
    if (display) {
        XFreeGC(display, gc);
        XCloseDisplay(display);
    }
}