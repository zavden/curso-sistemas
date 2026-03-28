#include <stdio.h>

int main(void) {
    const char *path = "numbers.bin";

    /* --- Write 5 ints to a binary file --- */
    FILE *fw = fopen(path, "wb");
    if (!fw) {
        perror(path);
        return 1;
    }

    int data[] = {10, 20, 30, 40, 50};
    fwrite(data, sizeof(int), 5, fw);
    fclose(fw);

    printf("Wrote 5 ints (%zu bytes) to %s\n\n", 5 * sizeof(int), path);

    /* --- Try to read 10 ints (more than available) --- */
    FILE *fr = fopen(path, "rb");
    if (!fr) {
        perror(path);
        return 1;
    }

    int buf[10];
    size_t got = fread(buf, sizeof(int), 10, fr);

    printf("Requested 10 ints, got %zu\n", got);
    printf("feof()   = %d\n", feof(fr) ? 1 : 0);
    printf("ferror() = %d\n", ferror(fr) ? 1 : 0);

    printf("\nValues read:\n");
    for (size_t i = 0; i < got; i++) {
        printf("  buf[%zu] = %d\n", i, buf[i]);
    }

    /* --- Demonstrate the WRONG way to use feof --- */
    printf("\n=== Common mistake: while(!feof(f)) ===\n\n");

    rewind(fr);
    int val;
    int reads = 0;
    while (!feof(fr)) {
        size_t r = fread(&val, sizeof(int), 1, fr);
        reads++;
        printf("  iteration %d: fread returned %zu, val=%d, feof=%d\n",
               reads, r, val, feof(fr) ? 1 : 0);
    }
    printf("Total iterations: %d (expected 5, got %d -- one extra!)\n",
           reads, reads);

    /* --- The CORRECT way --- */
    printf("\n=== Correct: check fread return value ===\n\n");

    rewind(fr);
    reads = 0;
    while (fread(&val, sizeof(int), 1, fr) == 1) {
        reads++;
        printf("  iteration %d: val=%d\n", reads, val);
    }
    printf("Total iterations: %d (correct!)\n", reads);

    if (feof(fr)) {
        printf("Stopped because: end of file\n");
    } else if (ferror(fr)) {
        printf("Stopped because: read error\n");
    }

    fclose(fr);
    return 0;
}
