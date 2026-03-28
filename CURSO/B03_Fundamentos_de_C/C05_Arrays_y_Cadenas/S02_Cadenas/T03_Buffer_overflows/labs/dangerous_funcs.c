#include <stdio.h>
#include <string.h>

/* --- Dangerous versions --- */

void dangerous_strcpy(const char *input) {
    char dest[16];
    strcpy(dest, input);
    printf("  strcpy result: %s\n", dest);
}

void dangerous_sprintf(const char *name, int age) {
    char buf[32];
    sprintf(buf, "Name: %s, Age: %d, Status: active user", name, age);
    printf("  sprintf result: %s\n", buf);
}

/* --- Safe versions --- */

void safe_strncpy(const char *input) {
    char dest[16];
    strncpy(dest, input, sizeof(dest) - 1);
    dest[sizeof(dest) - 1] = '\0';
    printf("  strncpy result: %s\n", dest);
}

void safe_snprintf(const char *name, int age) {
    char buf[32];
    int written = snprintf(buf, sizeof(buf),
                           "Name: %s, Age: %d, Status: active user",
                           name, age);
    printf("  snprintf result: %s\n", buf);
    if ((size_t)written >= sizeof(buf)) {
        printf("  WARNING: output was truncated (%d chars needed, %zu available)\n",
               written, sizeof(buf));
    }
}

int main(void) {
    const char *short_input = "Alice";
    const char *long_input  = "A very long username that exceeds the buffer";

    printf("=== Short input (safe for both) ===\n");
    printf("strcpy:\n");
    dangerous_strcpy(short_input);
    printf("strncpy:\n");
    safe_strncpy(short_input);

    printf("\n=== Long input (dangerous for strcpy) ===\n");
    printf("strncpy (safe):\n");
    safe_strncpy(long_input);

    printf("\n=== sprintf vs snprintf ===\n");
    printf("snprintf (safe):\n");
    safe_snprintf("Bartholomew Longname", 42);

    return 0;
}
