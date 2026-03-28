/* read_demo.c — Leer datos con read() */

#include <fcntl.h>      /* open, O_RDONLY */
#include <unistd.h>     /* read, write, close, STDOUT_FILENO */
#include <stdio.h>      /* perror, printf */

int main(int argc, char *argv[]) {
    if (argc != 2) {
        const char *usage = "Usage: ./read_demo <file>\n";
        write(STDERR_FILENO, usage, 26);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror(argv[1]);
        return 1;
    }

    printf("Opened %s: fd=%d\n", argv[1], fd);
    printf("Reading in loop until EOF...\n\n");

    char buf[32];   /* buffer pequeno para ver multiples iteraciones */
    ssize_t n;
    int iteration = 0;
    size_t total = 0;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        iteration++;
        total += (size_t)n;
        printf("[iter %d] read() returned %zd bytes\n", iteration, n);

        /* Escribir los datos leidos a stdout */
        write(STDOUT_FILENO, buf, (size_t)n);

        /* Separador visual despues de cada read */
        const char *sep = "\n--- end of chunk ---\n";
        write(STDOUT_FILENO, sep, 21);
    }

    if (n == -1) {
        perror("read");
        close(fd);
        return 1;
    }

    printf("\nEOF reached (read returned 0)\n");
    printf("Total: %zu bytes in %d iterations\n", total, iteration);

    close(fd);
    return 0;
}
