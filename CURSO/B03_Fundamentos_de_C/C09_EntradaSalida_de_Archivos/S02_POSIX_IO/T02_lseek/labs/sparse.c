#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define MB (1024L * 1024)

int main(void) {
    const char *path = "sparse.dat";

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); return 1; }

    /* Write "START" at byte 0 */
    const char *tag1 = "START";
    write(fd, tag1, strlen(tag1));
    printf("Wrote \"%s\" at offset 0\n", tag1);

    /* Seek to 50 MB and write "MIDDLE" */
    off_t mid = 50 * MB;
    lseek(fd, mid, SEEK_SET);
    const char *tag2 = "MIDDLE";
    write(fd, tag2, strlen(tag2));
    printf("Wrote \"%s\" at offset %ld\n", tag2, (long)mid);

    /* Seek to 100 MB - 3 and write "END" */
    off_t end_offset = 100 * MB - 3;
    lseek(fd, end_offset, SEEK_SET);
    const char *tag3 = "END";
    write(fd, tag3, strlen(tag3));
    printf("Wrote \"%s\" at offset %ld\n\n", tag3, (long)end_offset);

    /* Read back from each position to verify */
    char buf[16];

    lseek(fd, 0, SEEK_SET);
    read(fd, buf, 5);
    buf[5] = '\0';
    printf("Read at offset 0:        \"%s\"\n", buf);

    lseek(fd, mid, SEEK_SET);
    read(fd, buf, 6);
    buf[6] = '\0';
    printf("Read at offset %ld: \"%s\"\n", (long)mid, buf);

    lseek(fd, end_offset, SEEK_SET);
    read(fd, buf, 3);
    buf[3] = '\0';
    printf("Read at offset %ld: \"%s\"\n\n", (long)end_offset, buf);

    /* Read from a hole (should be all zeros) */
    lseek(fd, 1000, SEEK_SET);
    read(fd, buf, 8);
    printf("Read 8 bytes from hole (offset 1000):\n  ");
    for (int i = 0; i < 8; i++) {
        printf("0x%02x ", (unsigned char)buf[i]);
    }
    printf("\n  All zeros: %s\n\n",
           (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0 &&
            buf[4] == 0 && buf[5] == 0 && buf[6] == 0 && buf[7] == 0)
           ? "yes" : "no");

    /* Get logical size */
    off_t logical_size = lseek(fd, 0, SEEK_END);
    printf("Logical size: %ld bytes (~%ld MB)\n", (long)logical_size,
           (long)(logical_size / MB));

    close(fd);

    printf("\nUsa estos comandos para comparar tamano logico vs real:\n");
    printf("  ls -l %s\n", path);
    printf("  du -h %s\n", path);
    printf("  stat %s\n", path);

    return 0;
}
