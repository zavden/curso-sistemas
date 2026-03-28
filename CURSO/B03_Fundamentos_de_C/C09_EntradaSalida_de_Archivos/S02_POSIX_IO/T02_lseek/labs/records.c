#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

struct Record {
    int id;
    char name[50];
    double score;
};

static int write_record(int fd, int index, const struct Record *rec) {
    off_t offset = (off_t)index * (off_t)sizeof(struct Record);
    if (lseek(fd, offset, SEEK_SET) == -1) return -1;
    ssize_t n = write(fd, rec, sizeof(struct Record));
    return (n == (ssize_t)sizeof(struct Record)) ? 0 : -1;
}

static int read_record(int fd, int index, struct Record *rec) {
    off_t offset = (off_t)index * (off_t)sizeof(struct Record);
    if (lseek(fd, offset, SEEK_SET) == -1) return -1;
    ssize_t n = read(fd, rec, sizeof(struct Record));
    return (n == (ssize_t)sizeof(struct Record)) ? 0 : -1;
}

static int count_records(int fd) {
    off_t saved = lseek(fd, 0, SEEK_CUR);
    off_t size  = lseek(fd, 0, SEEK_END);
    lseek(fd, saved, SEEK_SET);
    return (int)(size / (off_t)sizeof(struct Record));
}

static void print_record(const struct Record *rec) {
    printf("  id=%-3d  name=%-20s  score=%.1f\n",
           rec->id, rec->name, rec->score);
}

int main(void) {
    const char *path = "records.dat";

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); return 1; }

    printf("sizeof(struct Record) = %zu bytes\n\n", sizeof(struct Record));

    /* Write 5 records sequentially */
    struct Record data[] = {
        { 1, "Alice",   9.5 },
        { 2, "Bob",     7.3 },
        { 3, "Charlie", 8.8 },
        { 4, "Diana",   6.1 },
        { 5, "Eve",     9.9 },
    };

    for (int i = 0; i < 5; i++) {
        write_record(fd, i, &data[i]);
    }

    printf("Wrote 5 records. Total in file: %d\n\n", count_records(fd));

    /* Read record at index 2 (Charlie) */
    struct Record rec;
    printf("--- Reading index 2 ---\n");
    read_record(fd, 2, &rec);
    print_record(&rec);

    /* Read record at index 0 (Alice) */
    printf("\n--- Reading index 0 ---\n");
    read_record(fd, 0, &rec);
    print_record(&rec);

    /* Update record at index 1 (Bob -> Robert, new score) */
    printf("\n--- Updating index 1 ---\n");
    struct Record updated = { 2, "Robert", 8.5 };
    write_record(fd, 1, &updated);
    printf("Updated.\n");

    /* Read all records to verify */
    printf("\n--- All records ---\n");
    int total = count_records(fd);
    for (int i = 0; i < total; i++) {
        read_record(fd, i, &rec);
        print_record(&rec);
    }

    close(fd);
    return 0;
}
