#include <stdio.h>

struct Rectangle {
    double width;
    double height;
};

void print_rect(const struct Rectangle *r) {
    printf("Rectangle(%.1f x %.1f)\n", r->width, r->height);
}

void scale_rect(struct Rectangle *r, double factor) {
    r->width *= factor;
    r->height *= factor;
}

double rect_area(const struct Rectangle *r) {
    return r->width * r->height;
}

int main(void) {
    struct Rectangle box = { .width = 5.0, .height = 3.0 };

    // Pointer to struct:
    struct Rectangle *p = &box;

    // Access with arrow operator (->):
    printf("width  = %.1f\n", p->width);
    printf("height = %.1f\n", p->height);

    // Equivalent verbose form -- (*p).member:
    printf("width  = %.1f (using (*p).width)\n", (*p).width);

    // Pass by const pointer (read-only):
    printf("\n");
    print_rect(&box);
    printf("area = %.1f\n", rect_area(&box));

    // Pass by pointer (allows modification):
    printf("\nScaling by 2x...\n");
    scale_rect(&box, 2.0);
    print_rect(&box);
    printf("area = %.1f\n", rect_area(&box));

    // Verify the original variable was modified:
    printf("\nbox.width  = %.1f (modified through pointer)\n", box.width);
    printf("box.height = %.1f (modified through pointer)\n", box.height);

    return 0;
}
