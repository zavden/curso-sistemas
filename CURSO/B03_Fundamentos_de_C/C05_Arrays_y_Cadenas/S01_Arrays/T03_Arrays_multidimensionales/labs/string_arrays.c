#include <stdio.h>
#include <string.h>

int main(void) {
    /* Array of pointers to string literals (read-only strings) */
    const char *names[] = {"Alice", "Bob", "Charlie", "Dave"};
    int n_names = (int)(sizeof(names) / sizeof(names[0]));

    printf("=== const char *names[] (array of pointers) ===\n");
    printf("sizeof(names)    = %zu bytes (%d pointers)\n",
           sizeof(names), n_names);
    printf("sizeof(names[0]) = %zu bytes (one pointer)\n\n",
           sizeof(names[0]));

    for (int i = 0; i < n_names; i++) {
        printf("names[%d] = %-10s  ptr: %p  strlen: %zu\n",
               i, names[i], (void *)names[i], strlen(names[i]));
    }

    /* 2D char array (fixed-size buffer per string) */
    char cities[][12] = {"Madrid", "Barcelona", "Valencia", "Sevilla"};
    int n_cities = (int)(sizeof(cities) / sizeof(cities[0]));

    printf("\n=== char cities[][12] (2D char array) ===\n");
    printf("sizeof(cities)    = %zu bytes (%d rows x 12 cols)\n",
           sizeof(cities), n_cities);
    printf("sizeof(cities[0]) = %zu bytes (one row = buffer)\n\n",
           sizeof(cities[0]));

    for (int i = 0; i < n_cities; i++) {
        printf("cities[%d] = %-12s  addr: %p  strlen: %zu  buffer: %zu\n",
               i, cities[i], (void *)cities[i], strlen(cities[i]),
               sizeof(cities[i]));
    }

    /* Key difference: cities can be modified, names (literals) cannot */
    printf("\n=== Modifying cities[0] ===\n");
    printf("Before: cities[0] = %s\n", cities[0]);
    strcpy(cities[0], "Bilbao");
    printf("After:  cities[0] = %s\n", cities[0]);

    /* names[0][0] = 'X';  <-- undefined behavior! (string literal) */
    printf("\nnames[0] points to a string literal (read-only).\n");
    printf("Modifying it would be undefined behavior.\n");

    return 0;
}
