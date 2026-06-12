#ifndef LUCIS_NET_H
#define LUCIS_NET_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* ptr;
    size_t      len;
} lucis_net_str_result;

/* TCP */
int32_t  lucis_tcpConnect(const char* host, size_t hostLen, uint16_t port);
int32_t  lucis_tcpListen(const char* host, size_t hostLen, uint16_t port);
int32_t  lucis_tcpAccept(int32_t fd);
int64_t  lucis_tcpSend(int32_t fd, const char* data, size_t dataLen);
lucis_net_str_result lucis_tcpRecv(int32_t fd, size_t maxLen);

/* UDP */
int32_t  lucis_udpBind(const char* host, size_t hostLen, uint16_t port);
int64_t  lucis_udpSendTo(int32_t fd, const char* data, size_t dataLen,
                           const char* host, size_t hostLen, uint16_t port);
lucis_net_str_result lucis_udpRecvFrom(int32_t fd, size_t maxLen);

/* General */
void     lucis_netClose(int32_t fd);
void     lucis_netSetTimeout(int32_t fd, uint64_t ms);
lucis_net_str_result lucis_netResolve(const char* host, size_t hostLen);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_NET_H */
