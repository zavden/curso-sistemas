/* greeting.h — Header en directorio separado (para demostrar -I) */

#ifndef GREETING_H
#define GREETING_H

#include <stdio.h>

static inline void greet(const char *name) {
    printf("Hello, %s!\n", name);
}

#endif /* GREETING_H */
