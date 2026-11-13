#pragma once

// Narrow Ring 0 socket interface. Keep AFD independent of the hosted TCP/IP
// implementation types and their C++ containers/ownership machinery.
#include <cstddef>
#include <cstdint>

namespace micant::tcpip::kernel_api {

bool hasAdapter() noexcept;
bool isInitialized() noexcept;
void* referenceSocketWaitObject(int socketId) noexcept;
void releaseSocketWaitObject(void* waitObject) noexcept;
void cancelSocketWaitObject(void* waitObject) noexcept;
int32_t waitSocketData(void* waitObject, uint32_t timeoutMilliseconds) noexcept;
int32_t waitSocketConnected(void* waitObject, uint32_t timeoutMilliseconds) noexcept;
int32_t waitSocketWritable(void* waitObject, uint32_t timeoutMilliseconds) noexcept;
int createSocket(int addressFamily, int socketType, int protocol) noexcept;
void closeSocket(int socketId) noexcept;
bool bindSocket(int socketId, uint32_t ipv4NetworkOrder, uint16_t port) noexcept;
bool connectSocket(int socketId, uint32_t ipv4NetworkOrder, uint16_t port, uint64_t deadline100ns) noexcept;
bool listenSocket(int socketId, int backlog) noexcept;
int sendSocket(int socketId, const void* data, size_t length, uint64_t deadline100ns) noexcept;
int sendToSocket(int socketId, const void* data, size_t length,
                 uint32_t ipv4NetworkOrder, uint16_t port, uint64_t deadline100ns) noexcept;
int recvSocket(int socketId, void* buffer, size_t capacity, bool* truncated = nullptr) noexcept;
int recvFromSocket(int socketId, void* buffer, size_t capacity,
                   uint32_t* sourceIpv4NetworkOrder, uint16_t* sourcePort,
                   bool* truncated = nullptr) noexcept;
void queryNetworkConfiguration(uint32_t* ipv4, uint32_t* mask, uint32_t* gateway,
                               uint32_t* dns, uint8_t* mac) noexcept;
bool setNetworkConfiguration(uint32_t ipv4, uint32_t mask, uint32_t gateway,
                             uint32_t dns) noexcept;

} // namespace micant::tcpip::kernel_api
