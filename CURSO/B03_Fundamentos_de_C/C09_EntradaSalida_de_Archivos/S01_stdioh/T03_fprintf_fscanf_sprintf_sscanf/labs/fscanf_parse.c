/* fscanf_parse.c — Parse formatted data from a file, check return values */

#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(void) {
    const char *filename = "sensors.dat";

    /* Create a data file to parse */
    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Error: cannot create %s: %s\n",
                filename, strerror(errno));
        return 1;
    }

    fprintf(f, "temp_cpu 72.5\n");
    fprintf(f, "temp_gpu 68.3\n");
    fprintf(f, "fan_rpm 1800\n");
    fprintf(f, "voltage 12.04\n");
    fprintf(f, "CORRUPTED LINE\n");
    fprintf(f, "temp_ssd 45.1\n");
    fclose(f);

    /* Now parse the file with fscanf */
    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open %s: %s\n",
                filename, strerror(errno));
        return 1;
    }

    printf("Parsing %s:\n\n", filename);

    char sensor_name[64];
    double value;
    int line_num = 0;
    int parsed_ok = 0;
    int parsed_fail = 0;

    /* fscanf returns the number of fields successfully read */
    int fields;
    while ((fields = fscanf(f, "%63s %lf", sensor_name, &value)) != EOF) {
        line_num++;

        if (fields == 2) {
            printf("  Line %d: sensor=%-12s value=%.2f  [OK: %d fields]\n",
                   line_num, sensor_name, value, fields);
            parsed_ok++;
        } else {
            printf("  Line %d: parse error (got %d fields, expected 2)  [SKIP]\n",
                   line_num, fields);
            parsed_fail++;

            /* Consume the rest of the bad line to avoid infinite loop */
            int ch;
            while ((ch = fgetc(f)) != '\n' && ch != EOF)
                ;
        }
    }

    printf("\nResult: %d parsed, %d failed, %d total\n",
           parsed_ok, parsed_fail, line_num);

    fclose(f);
    return 0;
}
