#include <stdio.h>

int main(void) {
    int number;
    int attempts = 0;

    do {
        printf("Enter a number between 1 and 10: ");
        if (scanf("%d", &number) != 1) {
            /* Clear invalid input */
            int c;
            while ((c = getchar()) != '\n' && c != EOF) {
                /* discard */
            }
            printf("Invalid input. Try again.\n");
            attempts++;
            continue;
        }
        attempts++;

        if (number < 1 || number > 10) {
            printf("%d is out of range. Try again.\n", number);
        }
    } while (number < 1 || number > 10);

    printf("\nYou entered: %d (after %d attempt%s)\n",
           number, attempts, attempts == 1 ? "" : "s");

    return 0;
}
