/* fd_inspect.c — Inspeccionar file descriptors del proceso */

#include <fcntl.h>      /* open, O_* flags */
#include <unistd.h>     /* close, STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO */
#include <stdio.h>      /* printf, perror */
#include <stdlib.h>     /* system */
#include <string.h>     /* snprintf */

int main(void) {
    /* --- Los 3 fds estandar --- */
    printf("Standard file descriptors:\n");
    printf("  STDIN_FILENO  = %d\n", STDIN_FILENO);
    printf("  STDOUT_FILENO = %d\n", STDOUT_FILENO);
    printf("  STDERR_FILENO = %d\n", STDERR_FILENO);
    printf("\n");

    /* --- Ver /proc/self/fd antes de abrir archivos --- */
    printf("=== /proc/self/fd (before opening files) ===\n");
    system("ls -l /proc/self/fd");
    printf("\n");

    /* --- Abrir dos archivos --- */
    int fd1 = open("fd_test_1.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    int fd2 = open("fd_test_2.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd1 == -1 || fd2 == -1) {
        perror("open");
        return 1;
    }

    printf("Opened fd_test_1.txt: fd=%d\n", fd1);
    printf("Opened fd_test_2.txt: fd=%d\n", fd2);
    printf("\n");

    /* --- Ver /proc/self/fd despues de abrir archivos --- */
    printf("=== /proc/self/fd (after opening 2 files) ===\n");
    system("ls -l /proc/self/fd");
    printf("\n");

    /* --- Cerrar el primer fd y abrir otro --- */
    close(fd1);
    printf("Closed fd=%d\n", fd1);

    int fd3 = open("fd_test_3.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd3 == -1) {
        perror("open fd_test_3.txt");
        close(fd2);
        return 1;
    }
    printf("Opened fd_test_3.txt: fd=%d\n", fd3);
    printf("(notice: fd %d was reused after closing it)\n", fd3);
    printf("\n");

    /* --- Estado final de /proc/self/fd --- */
    printf("=== /proc/self/fd (after close + reopen) ===\n");
    system("ls -l /proc/self/fd");

    /* --- Limpieza --- */
    close(fd2);
    close(fd3);

    return 0;
}
