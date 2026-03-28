#include <stdio.h>

int main(void) {
    int day = 3;

    // Basic switch with break
    printf("Day %d: ", day);
    switch (day) {
        case 1: printf("Monday\n");    break;
        case 2: printf("Tuesday\n");   break;
        case 3: printf("Wednesday\n"); break;
        case 4: printf("Thursday\n");  break;
        case 5: printf("Friday\n");    break;
        case 6: printf("Saturday\n");  break;
        case 7: printf("Sunday\n");    break;
        default: printf("Invalid\n");  break;
    }

    // Fall-through: grouping cases
    char c = 'e';
    printf("'%c' is a ", c);
    switch (c) {
        case 'a':
        case 'e':
        case 'i':
        case 'o':
        case 'u':
            printf("vowel\n");
            break;
        default:
            printf("consonant\n");
            break;
    }

    // Fall-through: accumulative behavior
    int level = 2;
    printf("Access level %d grants:\n", level);
    switch (level) {
        case 3:
            printf("  - admin panel\n");
            __attribute__((fallthrough));
        case 2:
            printf("  - write access\n");
            __attribute__((fallthrough));
        case 1:
            printf("  - read access\n");
            break;
        case 0:
            printf("  - no access\n");
            break;
        default:
            printf("  - unknown level\n");
            break;
    }

    return 0;
}
