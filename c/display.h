// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

int init_display(void);
void display_frame(const Vector3D* buffer);
void cleanup_display(void);

extern volatile int kanban;

#endif