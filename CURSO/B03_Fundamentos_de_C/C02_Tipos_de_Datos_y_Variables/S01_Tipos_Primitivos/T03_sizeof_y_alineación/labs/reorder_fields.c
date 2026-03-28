#include <stdio.h>
#include <stddef.h>
#include <stdalign.h>

/* Struct original: campos en orden arbitrario */
struct Wasteful {
    char   name;
    double salary;
    char   grade;
    int    id;
    char   dept;
    short  level;
};

/* Struct optimizado: campos ordenados de mayor a menor */
struct Optimized {
    double salary;
    int    id;
    short  level;
    char   name;
    char   grade;
    char   dept;
};

int main(void) {
    printf("=== struct Wasteful (orden arbitrario) ===\n");
    printf("sizeof:  %zu bytes\n", sizeof(struct Wasteful));
    printf("alignof: %zu\n", alignof(struct Wasteful));
    printf("  name:   offset=%2zu  size=%zu\n",
           offsetof(struct Wasteful, name), sizeof(char));
    printf("  salary: offset=%2zu  size=%zu\n",
           offsetof(struct Wasteful, salary), sizeof(double));
    printf("  grade:  offset=%2zu  size=%zu\n",
           offsetof(struct Wasteful, grade), sizeof(char));
    printf("  id:     offset=%2zu  size=%zu\n",
           offsetof(struct Wasteful, id), sizeof(int));
    printf("  dept:   offset=%2zu  size=%zu\n",
           offsetof(struct Wasteful, dept), sizeof(char));
    printf("  level:  offset=%2zu  size=%zu\n",
           offsetof(struct Wasteful, level), sizeof(short));

    printf("\n=== struct Optimized (mayor a menor) ===\n");
    printf("sizeof:  %zu bytes\n", sizeof(struct Optimized));
    printf("alignof: %zu\n", alignof(struct Optimized));
    printf("  salary: offset=%2zu  size=%zu\n",
           offsetof(struct Optimized, salary), sizeof(double));
    printf("  id:     offset=%2zu  size=%zu\n",
           offsetof(struct Optimized, id), sizeof(int));
    printf("  level:  offset=%2zu  size=%zu\n",
           offsetof(struct Optimized, level), sizeof(short));
    printf("  name:   offset=%2zu  size=%zu\n",
           offsetof(struct Optimized, name), sizeof(char));
    printf("  grade:  offset=%2zu  size=%zu\n",
           offsetof(struct Optimized, grade), sizeof(char));
    printf("  dept:   offset=%2zu  size=%zu\n",
           offsetof(struct Optimized, dept), sizeof(char));

    size_t sum = sizeof(char) * 3 + sizeof(double) + sizeof(int) + sizeof(short);
    printf("\n=== Comparacion ===\n");
    printf("Suma real de campos: %zu bytes\n", sum);
    printf("Wasteful:  %zu bytes (padding: %zu)\n",
           sizeof(struct Wasteful),
           sizeof(struct Wasteful) - sum);
    printf("Optimized: %zu bytes (padding: %zu)\n",
           sizeof(struct Optimized),
           sizeof(struct Optimized) - sum);
    printf("Ahorro: %zu bytes por struct\n",
           sizeof(struct Wasteful) - sizeof(struct Optimized));

    printf("\n=== En un array de 1000 elementos ===\n");
    printf("Wasteful[1000]:  %zu bytes\n", 1000 * sizeof(struct Wasteful));
    printf("Optimized[1000]: %zu bytes\n", 1000 * sizeof(struct Optimized));
    printf("Ahorro total:    %zu bytes\n",
           1000 * (sizeof(struct Wasteful) - sizeof(struct Optimized)));

    return 0;
}
