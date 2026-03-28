#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

static off_t get_file_size(int fd) {
    off_t saved = lseek(fd, 0, SEEK_CUR);   /* save current position */
    off_t size  = lseek(fd, 0, SEEK_END);    /* move to end */
    lseek(fd, saved, SEEK_SET);              /* restore position */
    return size;
}

int main(void) {
    const char *path = "size_test.dat";

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); return 1; }

    /* Write some data */
    const char *msg = "Hello, lseek!";
    write(fd, msg, strlen(msg));

    /* Get file size without losing our position */
    off_t pos_before = lseek(fd, 0, SEEK_CUR);
    off_t size = get_file_size(fd);
    off_t pos_after = lseek(fd, 0, SEEK_CUR);

    printf("File size:          %ld bytes\n", (long)size);
    printf("Position before:    %ld\n", (long)pos_before);
    printf("Position after:     %ld\n", (long)pos_after);
    printf("Position preserved: %s\n\n", pos_before == pos_after ? "yes" : "no");

    /* Write more data and check again */
    const char *extra = " More data here.";
    write(fd, extra, strlen(extra));

    size = get_file_size(fd);
    printf("After writing more:  %ld bytes\n", (long)size);
    printf("Expected:            %ld bytes\n",
           (long)(strlen(msg) + strlen(extra)));

    close(fd);
    return 0;
}
