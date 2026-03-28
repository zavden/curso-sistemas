#include <stdio.h>

int main(void) {
    int command = 1;

    printf("command = %d\n", command);
    printf("Executing: ");

    // BUG: missing break causes unintended fall-through
    switch (command) {
        case 1:
            printf("START ");
            // forgot break -- falls through to STOP!
        case 2:
            printf("STOP ");
            // forgot break -- falls through to RESET!
        case 3:
            printf("RESET ");
            break;
        default:
            printf("UNKNOWN ");
            break;
    }
    printf("\n");

    // What the programmer probably intended:
    printf("\nFixed version:\n");
    printf("command = %d\n", command);
    printf("Executing: ");

    switch (command) {
        case 1:
            printf("START ");
            break;
        case 2:
            printf("STOP ");
            break;
        case 3:
            printf("RESET ");
            break;
        default:
            printf("UNKNOWN ");
            break;
    }
    printf("\n");

    return 0;
}
