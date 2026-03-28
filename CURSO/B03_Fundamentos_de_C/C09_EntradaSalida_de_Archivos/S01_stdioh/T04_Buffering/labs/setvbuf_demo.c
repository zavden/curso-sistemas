#include <stdio.h>
#include <unistd.h>

int main(void) {
    FILE *f = fopen("setvbuf_output.txt", "w");
    if (f == NULL) {
        perror("fopen");
        return 1;
    }

    /* Change file buffering BEFORE any I/O on the stream */
    /* _IONBF = unbuffered: every fprintf does a write syscall */
    setvbuf(f, NULL, _IONBF, 0);

    for (int i = 0; i < 10; i++) {
        fprintf(f, "line %d\n", i);
    }

    fclose(f);
    printf("Wrote 10 lines unbuffered to setvbuf_output.txt\n");

    /* Now demonstrate changing stdout to unbuffered */
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("tick 1...");
    sleep(1);
    printf("tick 2...");
    sleep(1);
    printf("tick 3\n");
    /* With _IONBF, each printf appears immediately — no '\n' needed */

    return 0;
}
