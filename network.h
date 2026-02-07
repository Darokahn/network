#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>

typedef struct {
    struct pollfd server;
    struct sockaddr_in addr;
    struct pollfd* connections;
    int connectionCap;
    int connectionCount;
} server_t;

typedef int clientIter;

#define INITSIZE 5
#define QUEUEAMOUNT 5

static inline int startServerAddrInt(server_t* server, int addr, int port) {
    server->server.fd = socket(AF_INET, SOCK_STREAM, 0);
    server->server.events = POLLIN;
    server->connectionCount = 0;
    server->connections = NULL;
    if (server->server.fd == -1) {
        perror("socket failed");
        return errno;
    }
    server->addr = (struct sockaddr_in) {
        AF_INET,
        htons(port),
        htonl(addr),
        {0}
    };
    if (bind(server->server.fd, (struct sockaddr*) &server->addr, sizeof server->addr) == -1) {
        perror("bind failed");
        return errno;
    }
    if (listen(server->server.fd, QUEUEAMOUNT) == -1) {
        perror("listen failed");
        return errno;
    }
    return 0;
}

// This function's job is to inform the caller of how long the first valid ip is; therefore,
// conceptually, it cannot "fail". Every return value and behavior is it
// following the same rules. Therefore, 0 is not a "failure" sentinel;
// it is the function telling you how many bytes of the input were
// valid ip.
//
// The function should be modified to allow the trailing ones place to exceed 1 byte.
int popIpAddr(char* ip, int* out) {
    char* base = ip;
    int numbersFound = 0;
    unsigned int accum = 0;
    unsigned int lastNum = 0;
    char* lastGood = ip;
    while (numbersFound < 4 && *ip != 0) {
        int num, len;
        int success = sscanf(ip, "%d%n", &num, &len);
        if (success < 1) break;
        if (num >= 256 || num < 0) {
            ip = lastGood;
            break;
        }
        numbersFound++;
        ip += len;
        accum += lastNum;
        accum <<= 8;
        lastNum = num;
        lastGood = ip;
        if (*ip != '.') break;
        ip++;
    }
    if (numbersFound > 0) {
        accum <<= (8 * (4 - numbersFound));
        accum += lastNum;
    }
    else {
        if (strncmp(ip, "localhost", (sizeof "localhost") - 1) == 0) {
            accum = (127 << 24) + 1;
            ip += (sizeof "localhost") - 1;
        }
    }
    if (out != NULL) *out = accum;
    return ip - base;
}

int popPort(char* port, int* out) {
    int charsScanned = 0;
    int portNum = 0;
    int success = sscanf(port, "%d%n", &portNum, &charsScanned);
    if (!success || portNum < 0 || portNum > 65535) {
        return 0;
    }
    *out = portNum;
    return charsScanned;
}

static inline int startServerAddrStr(server_t* server, char* addr) {
    int addrNum;
    int port;
    int ipLen = popIpAddr(addr, &addrNum);
    char* base = addr;
    if (ipLen == 0) {
        fprintf(stderr, "failed to pop IP address from beginning of %s\n", addr);
        return -1;
    }
    addr += ipLen;
    if (*addr != ':') {
        int len = (int)(addr - base);
        fprintf(stderr, "expected ':' after %.*s\n", len, base);
        return -1;
    }
    addr++;
    int portLen = popPort(addr, &port);
    if (portLen == 0) {
        fprintf(stderr, "failed to pop port from beginning of %s\n", addr);
        return -1;
    }
    return startServerAddrInt(server, addrNum, port);
}

static inline int startServer(server_t* server, char* addr) {
    return startServerAddrStr(server, addr);
}

int addConnection(server_t* server, int connectionFd) {
    if (server->connections == NULL) {
        server->connections = malloc(INITSIZE * (sizeof *server->connections));
        server->connectionCap = INITSIZE;
    }
    if (server->connectionCount == server->connectionCap) {
        server->connectionCap *= 2;
        void* newPtr = realloc(server->connections, server->connectionCap * (sizeof * server->connections));
        if (newPtr == NULL) {
            perror("realloc failed");
            return errno;
        }
        server->connections = newPtr;
    }
    server->connections[server->connectionCount] = (struct pollfd) {
        connectionFd,
        POLLIN | POLLOUT,
        0
    };
    server->connectionCount++;
    return 0;
}

static inline int updateClients(server_t* server, int delay) {
    poll(&server->server, 1, 0);
    if (server->server.revents & POLLIN) {
        int newFd = accept(server->server.fd, NULL, NULL);
        if (newFd == -1) {
            perror("accept failed");
            return errno;
        }
        addConnection(server, newFd);
    }
    if (server->connectionCount > 0) {
        poll(server->connections, server->connectionCount, delay);
    }
    return 0;
}

// getReadyClient and closeClient work together, and position `iter` to cooperate with one another.
static inline struct pollfd getReadyClient(server_t* server, clientIter* iter) {
    for (;*iter < server->connectionCount; *iter += 1) {
        struct pollfd thisConnection = server->connections[*iter];
        if (!(thisConnection.revents & POLLIN)) continue;
        *iter += 1;
        return thisConnection;
    }
    *iter = -1;
    return (struct pollfd) {.fd=-1};
}

static inline int closeClientIter(server_t* server, clientIter* iter) {
    shutdown(server->connections[*iter - 1].fd, SHUT_RDWR);
    int status = close(server->connections[*iter - 1].fd);
    if (status == -1) {
        perror("close failed");
        return errno;
    }
    server->connections[*iter] = server->connections[server->connectionCount - 1];
    *iter -= 1;
    server->connectionCount--;
    return 0;
}
