#include <stdio.h>
#include "greeting.h"

int main(void)
{
    printf("=== Greeting program ===\n");
    greet("World");
    greet_loud("World");
    return 0;
}
