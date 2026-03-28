#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *path = "seek_test.dat";

    /* Create a file with known content */
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); return 1; }

    const char *text = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    ssize_t n = write(fd, text, strlen(text));
    if (n == -1) { perror("write"); close(fd); return 1; }

    printf("Wrote %zd bytes: \"%s\"\n\n", n, text);

    /* SEEK_SET: absolute position from the beginning */
    lseek(fd, 0, SEEK_SET);
    off_t pos = lseek(fd, 0, SEEK_CUR);
    printf("After lseek(fd, 0, SEEK_SET):  position = %ld\n", (long)pos);

    char buf[4];
    read(fd, buf, 3);
    buf[3] = '\0';
    printf("  read 3 bytes: \"%s\"\n\n", buf);

    /* SEEK_SET to byte 10 */
    lseek(fd, 10, SEEK_SET);
    pos = lseek(fd, 0, SEEK_CUR);
    printf("After lseek(fd, 10, SEEK_SET): position = %ld\n", (long)pos);

    read(fd, buf, 3);
    buf[3] = '\0';
    printf("  read 3 bytes: \"%s\"\n\n", buf);

    /* SEEK_CUR: relative to current position */
    /* After reading 3 bytes from position 10, we are at 13 */
    pos = lseek(fd, 0, SEEK_CUR);
    printf("Current position after read:   position = %ld\n", (long)pos);

    lseek(fd, 5, SEEK_CUR);
    pos = lseek(fd, 0, SEEK_CUR);
    printf("After lseek(fd, 5, SEEK_CUR):  position = %ld\n", (long)pos);

    read(fd, buf, 3);
    buf[3] = '\0';
    printf("  read 3 bytes: \"%s\"\n\n", buf);

    /* SEEK_CUR with negative offset (go backwards) */
    lseek(fd, -6, SEEK_CUR);
    pos = lseek(fd, 0, SEEK_CUR);
    printf("After lseek(fd, -6, SEEK_CUR): position = %ld\n", (long)pos);

    read(fd, buf, 3);
    buf[3] = '\0';
    printf("  read 3 bytes: \"%s\"\n\n", buf);

    /* SEEK_END: relative to end of file */
    lseek(fd, 0, SEEK_END);
    pos = lseek(fd, 0, SEEK_CUR);
    printf("After lseek(fd, 0, SEEK_END):  position = %ld\n", (long)pos);

    lseek(fd, -3, SEEK_END);
    pos = lseek(fd, 0, SEEK_CUR);
    printf("After lseek(fd, -3, SEEK_END): position = %ld\n", (long)pos);

    read(fd, buf, 3);
    buf[3] = '\0';
    printf("  read 3 bytes: \"%s\"\n\n", buf);

    close(fd);
    return 0;
}
