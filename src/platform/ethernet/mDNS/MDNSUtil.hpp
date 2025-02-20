// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "mdns/mdns.h"

namespace libobsensor {

/**
 * @brief mDNS util class
 */
class MDNSUtil {

public:
    static mdns_string_t ip_address_to_string(char *buffer, size_t capacity, const struct sockaddr *addr, size_t addrlen);

    static mdns_string_t ipv4_address_to_string(char *buffer, size_t capacity, const struct sockaddr_in *addr, size_t addrlen);

    static mdns_string_t ipv6_address_to_string(char *buffer, size_t capacity, const struct sockaddr_in6 *addr, size_t addrlen);
};

}  // namespace libobsensor
