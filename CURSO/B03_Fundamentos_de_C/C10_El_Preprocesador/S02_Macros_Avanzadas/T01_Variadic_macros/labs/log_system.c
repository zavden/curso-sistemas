/* log_system.c -- LOG macros with file, line, and conditional levels */

#include <stdio.h>
#include <stdlib.h>

/* Log levels: ERROR=1, WARN=2, INFO=3, DEBUG=4 */
#ifndef LOG_LEVEL
#define LOG_LEVEL 3  /* default: INFO (show ERROR, WARN, INFO) */
#endif

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] %s:%d: " fmt "\n", \
            __FILE__, __LINE__, ##__VA_ARGS__)

#if LOG_LEVEL >= 2
    #define LOG_WARN(fmt, ...) \
        fprintf(stderr, "[WARN]  %s:%d: " fmt "\n", \
                __FILE__, __LINE__, ##__VA_ARGS__)
#else
    #define LOG_WARN(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= 3
    #define LOG_INFO(fmt, ...) \
        fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_INFO(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= 4
    #define LOG_DEBUG(fmt, ...) \
        fprintf(stderr, "[DEBUG] %s:%d %s(): " fmt "\n", \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...) ((void)0)
#endif

void process_request(int id) {
    LOG_DEBUG("processing request");
    LOG_INFO("request %d received", id);

    if (id < 0) {
        LOG_ERROR("invalid request id: %d", id);
        return;
    }
    if (id == 0) {
        LOG_WARN("request id is zero, using default");
        id = 1;
    }

    LOG_DEBUG("request %d completed", id);
    LOG_INFO("done");
}

int main(void) {
    LOG_INFO("server starting");
    LOG_DEBUG("compiled with LOG_LEVEL=%d", LOG_LEVEL);

    process_request(5);
    process_request(0);
    process_request(-1);

    LOG_INFO("server shutting down");
    return 0;
}
