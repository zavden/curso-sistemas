/* sscanf_extract.c — Parse strings, extract fields, validate input */

#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(void) {
    /* --- Parse structured strings with sscanf --- */
    printf("--- Parsing structured strings ---\n\n");

    const char *log_line = "2026-03-18 ERROR Connection refused on port 8080";

    int year, month, day;
    char level[16];
    char message[128];

    /* %[^\n] reads everything until newline (like fgets within sscanf) */
    int fields = sscanf(log_line, "%d-%d-%d %15s %127[^\n]",
                        &year, &month, &day, level, message);

    printf("Input:   \"%s\"\n", log_line);
    printf("Fields:  %d (expected 5)\n", fields);
    if (fields == 5) {
        printf("Date:    %04d-%02d-%02d\n", year, month, day);
        printf("Level:   %s\n", level);
        printf("Message: %s\n", message);
    }

    /* --- Parse multiple lines with fgets + sscanf (safe pattern) --- */
    printf("\n--- fgets + sscanf pattern ---\n\n");

    const char *csv_file = "users.csv";
    FILE *f = fopen(csv_file, "w");
    if (!f) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return 1;
    }

    fprintf(f, "Alice,30,95.5\n");
    fprintf(f, "Bob,25,87.2\n");
    fprintf(f, "INVALID LINE\n");
    fprintf(f, "Charlie,35,91.8\n");
    fclose(f);

    f = fopen(csv_file, "r");
    if (!f) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return 1;
    }

    printf("%-10s %-5s %-8s %s\n", "NAME", "AGE", "SCORE", "STATUS");
    printf("%-10s %-5s %-8s %s\n", "----", "---", "-----", "------");

    char buf[256];
    int line = 0;
    while (fgets(buf, sizeof(buf), f)) {
        line++;

        /* Remove trailing newline */
        buf[strcspn(buf, "\n")] = '\0';

        char name[64];
        int age;
        double score;

        /* sscanf parses the already-read string: no buffer overflow risk */
        if (sscanf(buf, "%63[^,],%d,%lf", name, &age, &score) == 3) {
            printf("%-10s %-5d %-8.1f OK\n", name, age, score);
        } else {
            printf("%-10s %-5s %-8s PARSE ERROR (line %d: \"%s\")\n",
                   "???", "?", "?", line, buf);
        }
    }

    fclose(f);

    /* --- Validate return value to detect bad input --- */
    printf("\n--- Validation with sscanf return value ---\n\n");

    const char *inputs[] = {
        "192.168.1.100",
        "10.0.0.1",
        "not.an.ip.address",
        "256.1.2.3",
        "172.16",
    };
    int num_inputs = 5;

    for (int i = 0; i < num_inputs; i++) {
        int a, b, c, d;
        int n = sscanf(inputs[i], "%d.%d.%d.%d", &a, &b, &c, &d);

        printf("  \"%s\" -> %d fields", inputs[i], n);

        if (n == 4) {
            /* sscanf parsed 4 integers, but still need range check */
            if (a >= 0 && a <= 255 && b >= 0 && b <= 255 &&
                c >= 0 && c <= 255 && d >= 0 && d <= 255) {
                printf(" -> VALID (%d.%d.%d.%d)\n", a, b, c, d);
            } else {
                printf(" -> INVALID (octet out of range)\n");
            }
        } else {
            printf(" -> INVALID (expected 4 fields)\n");
        }
    }

    return 0;
}
