#include "network.h"

server_t server;

void closeGraceful(int sig) {
    close(server.server.fd);
    for (int i = 0; i < server.connectionCount; i++) {
        close(server.connections[i].fd);
    }
    exit(0);
}

char* defaultAddr = "localhost:3001";

int main(int argc, char* argv[]) {
    signal(SIGINT, closeGraceful);
    char* addr;
    if (argc <= 1) {
        printf("No address provided. Using %s\n", defaultAddr);
        addr = defaultAddr;
    }
    else addr = argv[1];
    if (startServer(&server, addr) != 0) exit(1);
    char buffer[1024];
    while (true) {
        updateClients(&server, 100);
        clientIter iter = 0;
        struct pollfd client;
        while ((client = getReadyClient(&server, &iter)).fd != -1) {
            char msg[] = "waow";
            int size = (sizeof msg) - 1;
            dprintf(client.fd, "HTTP/1.1 200 OK\n\rContent-Type: text/plain\n\rContent-Length: %d\r\n\r\n%s\n", size, msg);
            closeClientIter(&server, &iter);
        }
    }
}
