#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ── helpers ────────────────────────────────────────────────────────────── */

static lucis_net_str_result make_str(const char* p, size_t n) {
    return (lucis_net_str_result){ p, n };
}

static lucis_net_str_result empty_str(void) {
    char* p = (char*)malloc(1);
    p[0] = '\0';
    return make_str(p, 0);
}

/* Null-terminate a (ptr, len) string for POSIX APIs. */
static char* to_cstr(const char* s, size_t len) {
    char* buf = (char*)malloc(len + 1);
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

/* ── TCP ────────────────────────────────────────────────────────────────── */

int32_t lucis_tcpConnect(const char* host, size_t hostLen, uint16_t port) {
    char* h = to_cstr(host, hostLen);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%u", (unsigned)port);

    int rc = getaddrinfo(h, portStr, &hints, &res);
    free(h);
    if (rc != 0) return -1;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return (int32_t)fd;
}

int32_t lucis_tcpListen(const char* host, size_t hostLen, uint16_t port) {
    char* h = to_cstr(host, hostLen);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%u", (unsigned)port);

    int rc = getaddrinfo(hostLen > 0 ? h : NULL, portStr, &hints, &res);
    free(h);
    if (rc != 0) return -1;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);

    if (listen(fd, 128) < 0) {
        close(fd);
        return -1;
    }

    return (int32_t)fd;
}

int32_t lucis_tcpAccept(int32_t fd) {
    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    int client = accept(fd, (struct sockaddr*)&addr, &addrLen);
    return (int32_t)client;
}

int64_t lucis_tcpSend(int32_t fd, const char* data, size_t dataLen) {
    ssize_t sent = send(fd, data, dataLen, 0);
    return (int64_t)sent;
}

lucis_net_str_result lucis_tcpRecv(int32_t fd, size_t maxLen) {
    char* buf = (char*)malloc(maxLen);
    if (!buf) return empty_str();

    ssize_t n = recv(fd, buf, maxLen, 0);
    if (n <= 0) {
        free(buf);
        return empty_str();
    }

    return make_str(buf, (size_t)n);
}

/* ── UDP ────────────────────────────────────────────────────────────────── */

int32_t lucis_udpBind(const char* host, size_t hostLen, uint16_t port) {
    char* h = to_cstr(host, hostLen);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags    = AI_PASSIVE;

    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%u", (unsigned)port);

    int rc = getaddrinfo(hostLen > 0 ? h : NULL, portStr, &hints, &res);
    free(h);
    if (rc != 0) return -1;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    if (bind(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return (int32_t)fd;
}

int64_t lucis_udpSendTo(int32_t fd, const char* data, size_t dataLen,
                          const char* host, size_t hostLen, uint16_t port) {
    char* h = to_cstr(host, hostLen);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%u", (unsigned)port);

    int rc = getaddrinfo(h, portStr, &hints, &res);
    free(h);
    if (rc != 0) return -1;

    ssize_t sent = sendto(fd, data, dataLen, 0, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    return (int64_t)sent;
}

lucis_net_str_result lucis_udpRecvFrom(int32_t fd, size_t maxLen) {
    char* buf = (char*)malloc(maxLen);
    if (!buf) return empty_str();

    struct sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);
    ssize_t n = recvfrom(fd, buf, maxLen, 0, (struct sockaddr*)&addr, &addrLen);

    if (n <= 0) {
        free(buf);
        return empty_str();
    }

    return make_str(buf, (size_t)n);
}

/* ── General ────────────────────────────────────────────────────────────── */

void lucis_netClose(int32_t fd) {
    close(fd);
}

void lucis_netSetTimeout(int32_t fd, uint64_t ms) {
    struct timeval tv;
    tv.tv_sec  = (time_t)(ms / 1000);
    tv.tv_usec = (suseconds_t)((ms % 1000) * 1000);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

lucis_net_str_result lucis_netResolve(const char* host, size_t hostLen) {
    char* h = to_cstr(host, hostLen);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(h, NULL, &hints, &res);
    free(h);
    if (rc != 0) return empty_str();

    char ipBuf[INET6_ADDRSTRLEN];
    const char* ip = NULL;

    if (res->ai_family == AF_INET) {
        struct sockaddr_in* s = (struct sockaddr_in*)res->ai_addr;
        ip = inet_ntop(AF_INET, &s->sin_addr, ipBuf, sizeof(ipBuf));
    } else if (res->ai_family == AF_INET6) {
        struct sockaddr_in6* s = (struct sockaddr_in6*)res->ai_addr;
        ip = inet_ntop(AF_INET6, &s->sin6_addr, ipBuf, sizeof(ipBuf));
    }

    freeaddrinfo(res);

    if (!ip) return empty_str();

    size_t len = strlen(ip);
    char* out = (char*)malloc(len);
    memcpy(out, ip, len);
    return make_str(out, len);
}
