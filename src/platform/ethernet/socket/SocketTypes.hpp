// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
#include <winsock2.h>
#include <WS2tcpip.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#define SOCKET int
#define SOCKADDR sockaddr
#define SOCKADDR_IN sockaddr_in
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (-1)
#define TIMEVAL timeval
#define SD_BOTH SHUT_RDWR
#define closesocket close
#define ioctlsocket ioctl
#define TIMEVAL timeval
#define SD_BOTH SHUT_RDWR
#endif

namespace libobsensor {


#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
#define GET_LAST_ERROR() WSAGetLastError()
#else
#define GET_LAST_ERROR() errno
#endif


}  // namespace libobsensor

