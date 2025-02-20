// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <stdio.h>
#include <string.h>
#include "MDNSUtil.hpp"

namespace libobsensor {

mdns_string_t MDNSUtil::ipv4_address_to_string(char *buffer, size_t capacity, const struct sockaddr_in *addr, size_t addrlen) {
    char host[NI_MAXHOST]    = { 0 };
    char service[NI_MAXSERV] = { 0 };
    int  ret = getnameinfo((const struct sockaddr *)addr, (socklen_t)addrlen, host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
    int  len = 0;
    if(ret == 0) {
        if(addr->sin_port != 0) {
            len = snprintf(buffer, capacity, "%s:%s", host, service);
        }
        else {
            len = snprintf(buffer, capacity, "%s", host);
        }
    }
    if(len >= (int)capacity) {
        len = (int)capacity - 1;
    }
    mdns_string_t str;
    str.str    = buffer;
    str.length = len;
    return str;
}

mdns_string_t MDNSUtil::ipv6_address_to_string(char *buffer, size_t capacity, const struct sockaddr_in6 *addr, size_t addrlen) {
    char host[NI_MAXHOST]    = { 0 };
    char service[NI_MAXSERV] = { 0 };
    int  ret = getnameinfo((const struct sockaddr *)addr, (socklen_t)addrlen, host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
    int  len = 0;
    if(ret == 0) {
        if(addr->sin6_port != 0) {
            len = snprintf(buffer, capacity, "[%s]:%s", host, service);
        }
        else {
            len = snprintf(buffer, capacity, "%s", host);
        }
    }
    if(len >= (int)capacity) {
        len = (int)capacity - 1;
    }
    mdns_string_t str;
    str.str    = buffer;
    str.length = len;
    return str;
}

mdns_string_t MDNSUtil::ip_address_to_string(char *buffer, size_t capacity, const struct sockaddr *addr, size_t addrlen) {
    if(addr->sa_family == AF_INET6) {
        return ipv6_address_to_string(buffer, capacity, (const struct sockaddr_in6 *)addr, addrlen);
    }
    return ipv4_address_to_string(buffer, capacity, (const struct sockaddr_in *)addr, addrlen);
}

}  // namespace libobsensor
