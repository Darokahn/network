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

// This function's job is to inform the caller of how long the first valid ip is; therefore,
// conceptually, it cannot "fail". Every return value and behavior is it
// following the same rules. Therefore, 0 is not a "failure" sentinel;
// it is the function telling you how many bytes of the input were
// valid ip.
//
// TODO: The function should be modified to allow the trailing ones place to exceed 1 byte.
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
