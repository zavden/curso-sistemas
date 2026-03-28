/* bench_write.c -- Benchmark write performance: stdio vs POSIX with
 *                  different buffer sizes.
 *
 * Usage: ./bench_write
 *
 * Writes TOTAL_BYTES to a temporary file using 4 strategies:
 *   1. write() with 1-byte buffer    (many syscalls)
 *   2. write() with 4096-byte buffer (fewer syscalls)
 *   3. fputc() one byte at a time    (stdio buffered internally)
 *   4. fwrite() with 4096-byte buffer (stdio + large writes)
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define TOTAL_BYTES (4 * 1024 * 1024)  /* 4 MB */
#define BUF_LARGE   4096

static double elapsed_ms(struct timespec *start, struct timespec *end)
{
    double sec  = (double)(end->tv_sec  - start->tv_sec);
    double nsec = (double)(end->tv_nsec - start->tv_nsec);
    return (sec * 1000.0) + (nsec / 1000000.0);
}

/* Strategy 1: write() with 1-byte buffer */
static double bench_write_1byte(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); exit(1); }

    char c = 'X';
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < TOTAL_BYTES; i++) {
        write(fd, &c, 1);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    close(fd);
    return elapsed_ms(&start, &end);
}

/* Strategy 2: write() with 4096-byte buffer */
static double bench_write_4k(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); exit(1); }

    char buf[BUF_LARGE];
    memset(buf, 'X', sizeof(buf));

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int remaining = TOTAL_BYTES;
    while (remaining > 0) {
        int chunk = remaining < BUF_LARGE ? remaining : BUF_LARGE;
        write(fd, buf, (size_t)chunk);
        remaining -= chunk;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    close(fd);
    return elapsed_ms(&start, &end);
}

/* Strategy 3: fputc() one byte at a time */
static double bench_fputc(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror("fopen"); exit(1); }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < TOTAL_BYTES; i++) {
        fputc('X', f);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    fclose(f);
    return elapsed_ms(&start, &end);
}

/* Strategy 4: fwrite() with 4096-byte buffer */
static double bench_fwrite_4k(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror("fopen"); exit(1); }

    char buf[BUF_LARGE];
    memset(buf, 'X', sizeof(buf));

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int remaining = TOTAL_BYTES;
    while (remaining > 0) {
        int chunk = remaining < BUF_LARGE ? remaining : BUF_LARGE;
        fwrite(buf, 1, (size_t)chunk, f);
        remaining -= chunk;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    fclose(f);
    return elapsed_ms(&start, &end);
}

int main(void)
{
    const char *tmpfile = "bench_tmp.dat";

    printf("Writing %d bytes (%.1f MB) per strategy...\n\n",
           TOTAL_BYTES, (double)TOTAL_BYTES / (1024.0 * 1024.0));

    printf("%-35s %10s %12s\n", "Strategy", "Time (ms)", "Syscalls (approx)");
    printf("%-35s %10s %12s\n", "-----------------------------------",
           "----------", "------------");

    double t1 = bench_write_1byte(tmpfile);
    printf("%-35s %10.1f %12d\n",
           "1. write() 1 byte at a time", t1, TOTAL_BYTES);

    double t2 = bench_write_4k(tmpfile);
    printf("%-35s %10.1f %12d\n",
           "2. write() 4096-byte buffer", t2, TOTAL_BYTES / BUF_LARGE);

    double t3 = bench_fputc(tmpfile);
    printf("%-35s %10.1f %12s\n",
           "3. fputc() 1 byte at a time", t3, "~512");

    double t4 = bench_fwrite_4k(tmpfile);
    printf("%-35s %10.1f %12s\n",
           "4. fwrite() 4096-byte buffer", t4, "~1024");

    printf("\nRatio write(1 byte) / write(4096): %.1fx slower\n", t1 / t2);
    printf("Ratio write(1 byte) / fputc():     %.1fx slower\n", t1 / t3);

    /* Cleanup */
    unlink(tmpfile);

    return 0;
}
