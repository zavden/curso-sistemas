#include <stdio.h>

int main(void) {
    int arr[5] = {10, 20, 30, 40, 50};

    printf("arr[0] = %d\n", arr[0]);
    printf("arr[4] = %d\n", arr[4]);

    /* Out-of-bounds access - Undefined Behavior */
    /* The compiler may not warn, but the result is unpredictable */
    printf("arr[5] = %d  (UB: past the end)\n", arr[5]);
    printf("arr[10] = %d  (UB: far past the end)\n", arr[10]);

    return 0;
}
