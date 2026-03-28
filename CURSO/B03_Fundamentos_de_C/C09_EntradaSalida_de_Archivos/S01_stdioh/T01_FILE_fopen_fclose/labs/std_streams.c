/* std_streams.c -- Demonstrate stdin, stdout, and stderr */

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* stdout -- normal output (line-buffered to terminal) */
    fprintf(stdout, "This goes to stdout\n");

    /* stderr -- error output (unbuffered) */
    fprintf(stderr, "This goes to stderr\n");

    /* printf is shorthand for fprintf(stdout, ...) */
    printf("printf is equivalent to fprintf(stdout, ...)\n");

    /* Show that stdout and stderr are separate streams */
    fprintf(stdout, "stdout: order test 1\n");
    fprintf(stderr, "stderr: order test 2\n");
    fprintf(stdout, "stdout: order test 3\n");

    return EXIT_SUCCESS;
}
