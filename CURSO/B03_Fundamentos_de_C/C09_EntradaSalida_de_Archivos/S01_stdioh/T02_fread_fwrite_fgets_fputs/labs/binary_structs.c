#include <stdio.h>
#include <string.h>

struct Sensor {
    int id;
    char label[32];
    double value;
};

static void print_sensor(const struct Sensor *s) {
    printf("  id=%-3d  label=%-12s  value=%.2f\n", s->id, s->label, s->value);
}

int main(void) {
    const char *path = "sensors.bin";

    /* --- Write structs with fwrite --- */
    struct Sensor data[] = {
        {1,  "temperature", 23.5},
        {2,  "humidity",    61.2},
        {3,  "pressure",  1013.25},
    };
    int count = sizeof(data) / sizeof(data[0]);

    FILE *fw = fopen(path, "wb");
    if (!fw) {
        perror(path);
        return 1;
    }

    fwrite(&count, sizeof(int), 1, fw);
    size_t written = fwrite(data, sizeof(struct Sensor), count, fw);
    printf("Wrote %zu of %d structs to %s\n", written, count, path);
    printf("sizeof(struct Sensor) = %zu bytes\n", sizeof(struct Sensor));
    printf("Total file size = %zu bytes (approx)\n\n",
           sizeof(int) + count * sizeof(struct Sensor));
    fclose(fw);

    /* --- Read structs with fread --- */
    FILE *fr = fopen(path, "rb");
    if (!fr) {
        perror(path);
        return 1;
    }

    int n = 0;
    fread(&n, sizeof(int), 1, fr);
    printf("File says it contains %d structs\n", n);

    struct Sensor loaded[10];
    size_t got = fread(loaded, sizeof(struct Sensor), n, fr);
    fclose(fr);

    printf("Read %zu structs:\n", got);
    for (int i = 0; i < (int)got; i++) {
        print_sensor(&loaded[i]);
    }

    /* --- Verify data matches --- */
    printf("\nVerification: ");
    if (memcmp(data, loaded, count * sizeof(struct Sensor)) == 0) {
        printf("PASS - data matches exactly\n");
    } else {
        printf("FAIL - data does not match\n");
    }

    return 0;
}
