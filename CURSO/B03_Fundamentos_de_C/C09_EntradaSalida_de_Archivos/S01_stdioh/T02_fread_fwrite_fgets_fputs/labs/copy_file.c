#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Copy byte-by-byte using fgetc/fputc */
static long copy_byte_by_byte(const char *src, const char *dst) {
    FILE *fin = fopen(src, "rb");
    FILE *fout = fopen(dst, "wb");
    if (!fin || !fout) {
        if (fin) fclose(fin);
        if (fout) fclose(fout);
        return -1;
    }

    long bytes = 0;
    int ch;
    while ((ch = fgetc(fin)) != EOF) {
        fputc(ch, fout);
        bytes++;
    }

    fclose(fin);
    fclose(fout);
    return bytes;
}

/* Copy in blocks using fread/fwrite */
static long copy_block(const char *src, const char *dst, size_t block_size) {
    FILE *fin = fopen(src, "rb");
    FILE *fout = fopen(dst, "wb");
    if (!fin || !fout) {
        if (fin) fclose(fin);
        if (fout) fclose(fout);
        return -1;
    }

    unsigned char *buf = malloc(block_size);
    if (!buf) {
        fclose(fin);
        fclose(fout);
        return -1;
    }

    long bytes = 0;
    size_t n;
    while ((n = fread(buf, 1, block_size, fin)) > 0) {
        fwrite(buf, 1, n, fout);
        bytes += (long)n;
    }

    free(buf);
    fclose(fin);
    fclose(fout);
    return bytes;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source_file>\n", argv[0]);
        return 1;
    }

    const char *src = argv[1];
    const char *dst_byte = "copy_byte.tmp";
    const char *dst_block = "copy_block.tmp";

    /* --- Byte-by-byte copy --- */
    clock_t t0 = clock();
    long b1 = copy_byte_by_byte(src, dst_byte);
    clock_t t1 = clock();

    if (b1 < 0) {
        perror(src);
        return 1;
    }

    double ms_byte = (double)(t1 - t0) / CLOCKS_PER_SEC * 1000.0;
    printf("Byte-by-byte: %ld bytes in %.2f ms\n", b1, ms_byte);

    /* --- Block copy (4096 bytes) --- */
    clock_t t2 = clock();
    long b2 = copy_block(src, dst_block, 4096);
    clock_t t3 = clock();

    double ms_block = (double)(t3 - t2) / CLOCKS_PER_SEC * 1000.0;
    printf("Block (4096): %ld bytes in %.2f ms\n", b2, ms_block);

    if (ms_byte > 0.0) {
        printf("Speedup:      %.1fx faster with block copy\n", ms_byte / ms_block);
    }

    return 0;
}
