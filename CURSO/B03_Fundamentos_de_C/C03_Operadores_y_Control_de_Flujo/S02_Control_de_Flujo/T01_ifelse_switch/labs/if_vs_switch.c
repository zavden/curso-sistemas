#include <stdio.h>

// Version using if/else if chain
const char *classify_if(int code) {
    if (code == 1) {
        return "OK";
    } else if (code == 2) {
        return "WARNING";
    } else if (code == 3) {
        return "ERROR";
    } else if (code == 4) {
        return "CRITICAL";
    } else if (code == 5) {
        return "FATAL";
    } else if (code == 6) {
        return "UNKNOWN";
    } else if (code == 7) {
        return "TIMEOUT";
    } else if (code == 8) {
        return "RETRY";
    } else {
        return "INVALID";
    }
}

// Version using switch
const char *classify_switch(int code) {
    switch (code) {
        case 1: return "OK";
        case 2: return "WARNING";
        case 3: return "ERROR";
        case 4: return "CRITICAL";
        case 5: return "FATAL";
        case 6: return "UNKNOWN";
        case 7: return "TIMEOUT";
        case 8: return "RETRY";
        default: return "INVALID";
    }
}

int main(void) {
    for (int i = 0; i <= 9; i++) {
        printf("code %d: if=%s, switch=%s\n",
               i, classify_if(i), classify_switch(i));
    }
    return 0;
}
