/* write_demo.c — Escribir datos con write() */

#include <fcntl.h>      /* open, O_* flags */
#include <unistd.h>     /* write, close, STDOUT_FILENO */
#include <stdio.h>      /* perror, printf */
#include <string.h>     /* strlen */
#include <errno.h>       /* errno, EINTR */

/* write_all: loop para manejar short writes */
static ssize_t write_all(int fd, const void *buf, size_t count) {
    const char *p = buf;
    size_t remaining = count;

    while (remaining > 0) {
        ssize_t n = write(fd, p, remaining);
        if (n == -1) {
            if (errno == EINTR) continue;   /* interrupted by signal */
            return -1;
        }
        p += n;
        remaining -= (size_t)n;
    }
    return (ssize_t)count;
}

int main(void) {
    /* --- write a stdout --- */
    const char *msg = "Hello from write()!\n";
    ssize_t n = write(STDOUT_FILENO, msg, strlen(msg));
    if (n == -1) {
        perror("write stdout");
        return 1;
    }
    printf("write() returned: %zd (expected %zu)\n", n, strlen(msg));

    /* --- write a un archivo --- */
    int fd = open("written.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open written.txt");
        return 1;
    }

    const char *lines[] = {
        "Line 1: POSIX I/O\n",
        "Line 2: open/close/read/write\n",
        "Line 3: file descriptors\n"
    };

    for (int i = 0; i < 3; i++) {
        ssize_t written = write_all(fd, lines[i], strlen(lines[i]));
        if (written == -1) {
            perror("write_all");
            close(fd);
            return 1;
        }
        printf("Wrote %zd bytes to fd=%d\n", written, fd);
    }

    close(fd);
    printf("Data written to written.txt\n");

    return 0;
}
