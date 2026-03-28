#include <stdio.h>

struct Permissions {
    unsigned int read    : 1;
    unsigned int write   : 1;
    unsigned int execute : 1;
};

struct PackedDate {
    unsigned int day   : 5;
    unsigned int month : 4;
    unsigned int year  : 7;
};

int main(void) {
    struct Permissions perms = { .read = 1, .write = 1, .execute = 0 };

    printf("=== Bit field de 1 bit ===\n");
    printf("read=%u  write=%u  exec=%u\n",
           perms.read, perms.write, perms.execute);
    printf("sizeof(struct Permissions) = %zu\n\n", sizeof(struct Permissions));

    struct PackedDate date = { .day = 18, .month = 3, .year = 126 };

    printf("=== Bit fields de 5, 4 y 7 bits ===\n");
    printf("day=%u  month=%u  year=%u\n", date.day, date.month, date.year);
    printf("Fecha: %u/%u/%u\n", date.day, date.month, date.year + 1900);
    printf("sizeof(struct PackedDate) = %zu\n\n", sizeof(struct PackedDate));

    printf("=== Comparacion de tamano ===\n");
    struct RegularDate {
        int day;
        int month;
        int year;
    };
    printf("sizeof(struct PackedDate)   = %zu bytes\n", sizeof(struct PackedDate));
    printf("sizeof(struct RegularDate)  = %zu bytes\n", sizeof(struct RegularDate));

    return 0;
}
