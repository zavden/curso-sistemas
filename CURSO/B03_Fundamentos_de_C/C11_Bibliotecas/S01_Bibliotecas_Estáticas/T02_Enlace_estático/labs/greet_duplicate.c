#include <stdio.h>

/* Duplicate definition of greet_person -- used to provoke
   "multiple definition" linker error in Part 5. */
void greet_person(const char *name)
{
    printf("Duplicate greet: %s\n", name);
}
