#include "greet.h"
#include <stdio.h>

int main(void)
{
    printf("=== Static linking demo ===\n");
    greet_person("Alice");
    greet_person("Bob");
    return 0;
}
