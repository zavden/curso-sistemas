#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void handle_sigterm(int sig) {
    (void)sig;
    printf("[PID %d] SIGTERM received — starting graceful shutdown...\n", getpid());
    fflush(stdout);
    running = 0;
}

int main(void) {
    struct sigaction sa;
    sa.sa_handler = handle_sigterm;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    printf("[PID %d] Signal handler registered for SIGTERM\n", getpid());
    printf("[PID %d] Running... send SIGTERM to trigger graceful shutdown\n", getpid());
    fflush(stdout);

    while (running) {
        sleep(1);
    }

    printf("[PID %d] Cleaning up resources...\n", getpid());
    fflush(stdout);
    sleep(2);

    printf("[PID %d] Cleanup complete. Exiting with code 0.\n", getpid());
    fflush(stdout);

    return 0;
}
