/* open_close.c — Abrir y cerrar archivos con open/close */

#include <fcntl.h>      /* open, O_* flags */
#include <unistd.h>     /* close */
#include <stdio.h>      /* perror, printf */
#include <errno.h>      /* errno, ENOENT, EEXIST */
#include <string.h>     /* strerror */

int main(void) {
    int fd;

    /* --- Abrir un archivo que no existe --- */
    fd = open("nonexistent.txt", O_RDONLY);
    if (fd == -1) {
        printf("open failed: errno=%d (%s)\n", errno, strerror(errno));
        /* No salimos: es el comportamiento esperado */
    }

    /* --- Crear un archivo para escritura --- */
    fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open output.txt");
        return 1;
    }
    printf("Opened output.txt: fd=%d\n", fd);

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }
    printf("Closed fd=%d\n", fd);

    /* --- Crear con O_EXCL (falla si ya existe) --- */
    fd = open("output.txt", O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd == -1 && errno == EEXIST) {
        printf("O_EXCL: file already exists (errno=%d: %s)\n",
               errno, strerror(errno));
    } else if (fd == -1) {
        perror("open O_EXCL");
        return 1;
    } else {
        close(fd);
    }

    /* --- Abrir para lectura el archivo creado --- */
    fd = open("output.txt", O_RDONLY);
    if (fd == -1) {
        perror("open output.txt (read)");
        return 1;
    }
    printf("Opened output.txt for reading: fd=%d\n", fd);
    close(fd);

    return 0;
}
