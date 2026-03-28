#include <stdio.h>

int main(void) {
    int data[] = {4, 7, 2, -3, 8, -1, 6};
    int len = (int)(sizeof(data) / sizeof(data[0]));

    printf("Array: ");
    for (int i = 0; i < len; i++) {
        printf("%d ", data[i]);
    }
    printf("\n\n");

    /* break: find first negative */
    printf("--- break: find first negative ---\n");
    for (int i = 0; i < len; i++) {
        if (data[i] < 0) {
            printf("First negative: %d at index %d\n", data[i], i);
            break;
        }
    }

    /* continue: skip negatives */
    printf("\n--- continue: print only positives ---\n");
    for (int i = 0; i < len; i++) {
        if (data[i] < 0) {
            continue;
        }
        printf("%d ", data[i]);
    }
    printf("\n");

    return 0;
}
