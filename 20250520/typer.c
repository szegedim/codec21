#include <stdio.h>
#include <string.h>
#include <ctype.h>

/*
gcc typer.c -o typer; ./typer < sample_input.txt
*/

// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

int main() {
    char line[256];
    int shift_active_for_next = 0; // Flag to indicate if "shift Shift." was the last command

    while (fgets(line, sizeof(line), stdin)) {
        // Remove trailing newline character if present
        line[strcspn(line, "\n")] = 0;

        if (strstr(line, "Typed characters ") != NULL) {
            char *typed_part = strstr(line, "Typed characters ") + strlen("Typed characters ");

            if (strcmp(typed_part, "shift Shift.") == 0) {
                // This command sets shift for the *next* character if it's a simple one,
                // but is ignored if the next command is "shift X."
                shift_active_for_next = 1;
            } else if (strcmp(typed_part, "Enter.") == 0) {
                putchar('\n');
                shift_active_for_next = 0; // Reset shift state
            } else if (strcmp(typed_part, " .") == 0) { // Double space
                putchar(' ');
                shift_active_for_next = 0; // Reset shift state
            } else if (strncmp(typed_part, "shift ", strlen("shift ")) == 0 &&
                       strlen(typed_part) == strlen("shift ") + 2 &&
                       typed_part[strlen(typed_part) - 1] == '.') {
                // Handles "shift X." directly (e.g., "shift H.", "shift !.")
                char c = typed_part[strlen("shift ")];
                putchar(toupper(c));
                shift_active_for_next = 0; // This type of shift doesn't carry over
            } else if (strlen(typed_part) == 2 && typed_part[1] == '.') {
                // Handles single characters like "e."
                char c = typed_part[0];
                if (shift_active_for_next) {
                    putchar(toupper(c));
                } else {
                    putchar(c);
                }
                shift_active_for_next = 0; // Reset shift state after typing the character
            }
            // Other "Typed characters" variants not explicitly handled are ignored.
        }
        // Lines starting with "Click at coordinates" or other prefixes are ignored.
    }

    return 0;
}