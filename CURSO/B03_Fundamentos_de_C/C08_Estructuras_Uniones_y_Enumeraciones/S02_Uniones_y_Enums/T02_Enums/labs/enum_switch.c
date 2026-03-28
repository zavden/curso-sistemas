#include <stdio.h>

enum Color {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
};

static const char *color_names[] = {
    [COLOR_RED]   = "red",
    [COLOR_GREEN] = "green",
    [COLOR_BLUE]  = "blue",
};

const char *color_describe(enum Color c) {
    switch (c) {
        case COLOR_RED:   return "warm";
        case COLOR_GREEN: return "cool";
        case COLOR_BLUE:  return "cool";
    }
    return "unknown";
}

int main(void) {
    printf("--- Array indexado por enum ---\n");
    printf("COLOR_RED   (%d) -> %s\n", COLOR_RED,   color_names[COLOR_RED]);
    printf("COLOR_GREEN (%d) -> %s\n", COLOR_GREEN, color_names[COLOR_GREEN]);
    printf("COLOR_BLUE  (%d) -> %s\n", COLOR_BLUE,  color_names[COLOR_BLUE]);

    printf("\n--- Switch sobre enum ---\n");
    enum Color colors[] = { COLOR_RED, COLOR_GREEN, COLOR_BLUE };
    for (int i = 0; i < 3; i++) {
        printf("%s is %s\n", color_names[colors[i]], color_describe(colors[i]));
    }

    return 0;
}
