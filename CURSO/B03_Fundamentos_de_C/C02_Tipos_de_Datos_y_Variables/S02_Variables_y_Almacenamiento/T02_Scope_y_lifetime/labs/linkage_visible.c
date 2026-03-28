#include <stdio.h>

int public_var = 10;
static int private_var = 20;

void public_func(void) {
    printf("public_func: public_var=%d, private_var=%d\n",
           public_var, private_var);
}

static void private_func(void) {
    printf("private_func: only callable from this file\n");
}

/* Call private_func from a public wrapper so it is used */
void call_private(void) {
    private_func();
}
