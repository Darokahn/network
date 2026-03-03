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
        while ((client = getClientIter(&server, &iter)).fd != -1) {
            int count = read(client.fd, buffer, sizeof buffer);
            buffer[count] = 0;
            char msg[] = "Got your message.\n";
            dprintf(client.fd, "%s", msg);
            printf("%d says: %s\n", client.fd, buffer);
            //closeClientIter(&server, &iter);
        }
    }
}
