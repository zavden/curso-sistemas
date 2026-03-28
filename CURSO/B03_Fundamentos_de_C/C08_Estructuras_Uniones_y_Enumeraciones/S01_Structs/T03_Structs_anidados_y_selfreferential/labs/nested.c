#include <stdio.h>

struct Date {
    int year;
    int month;
    int day;
};

struct Address {
    char street[100];
    char city[50];
    int zip_code;
};

struct Employee {
    char name[50];
    struct Date birthday;
    struct Date hire_date;
    struct Address address;
};

int main(void) {
    struct Employee emp = {
        .name = "Alice Johnson",
        .birthday = { .year = 1990, .month = 5, .day = 15 },
        .hire_date = { .year = 2020, .month = 3, .day = 1 },
        .address = {
            .street = "742 Evergreen Terrace",
            .city = "Springfield",
            .zip_code = 62704,
        },
    };

    /* Access with double dot */
    printf("Name: %s\n", emp.name);
    printf("Born: %d-%02d-%02d\n",
           emp.birthday.year,
           emp.birthday.month,
           emp.birthday.day);
    printf("Hired: %d-%02d-%02d\n",
           emp.hire_date.year,
           emp.hire_date.month,
           emp.hire_date.day);
    printf("City: %s\n", emp.address.city);

    /* Access via pointer */
    struct Employee *p = &emp;
    printf("\nVia pointer:\n");
    printf("Name: %s\n", p->name);
    printf("Born: %d-%02d-%02d\n",
           p->birthday.year,
           p->birthday.month,
           p->birthday.day);
    printf("Address: %s, %s %d\n",
           p->address.street,
           p->address.city,
           p->address.zip_code);

    /* sizeof each struct */
    printf("\nsizeof(struct Date)     = %zu\n", sizeof(struct Date));
    printf("sizeof(struct Address)  = %zu\n", sizeof(struct Address));
    printf("sizeof(struct Employee) = %zu\n", sizeof(struct Employee));

    return 0;
}
