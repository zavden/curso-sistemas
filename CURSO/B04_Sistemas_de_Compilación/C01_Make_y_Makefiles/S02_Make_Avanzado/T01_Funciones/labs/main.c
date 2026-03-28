#include <stdio.h>
#include "engine.h"
#include "parser.h"
#include "client.h"
#include "server.h"

int main(void) {
    printf("=== Function Demo ===\n");
    engine_start();
    parser_run();
    client_connect();
    server_listen();
    printf("=== Done ===\n");
    return 0;
}
