#pragma once

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))

#include <dylib.hpp>
#include <memory>
#include <string>
#include <mutex>

#include "pcap/pcap.h"

namespace libobsensor {

typedef int (*pcap_findalldevs_t)(pcap_if_t **, char *);
typedef pcap_t *(*pcap_open_live_t)(const char *, int, int, int, char *);
typedef int (*pcap_datalink_t)(pcap_t *);
typedef int (*pcap_setfilter_t)(pcap_t *, struct bpf_program *);
typedef int (*pcap_compile_t)(pcap_t *, bpf_program *, const char *, int, bpf_u_int32);
typedef void (*pcap_freecode_t)(bpf_program *);
typedef void (*pcap_freealldevs_t)(pcap_if_t *);
typedef void (*pcap_close_t)(pcap_t *);
typedef int (*pcap_next_ex_t)(pcap_t *, struct pcap_pkthdr **, const u_char **);
typedef char (*pcap_geterr_t)(pcap_t *);

class NpCapLoader {

public:
    static NpCapLoader &getInstance();

    bool init();

    void unload();

private:
    NpCapLoader();
    ~NpCapLoader();

    NpCapLoader(const NpCapLoader &) = delete;
    NpCapLoader &operator=(const NpCapLoader &) = delete;

private:
    std::mutex             loadMtx_;
    std::shared_ptr<dylib> dylib_;
    bool                   wpcapDllLoaded_;

public:
    pcap_findalldevs_t pcap_findalldevs_;
    pcap_open_live_t   pcap_open_live_;
    pcap_datalink_t    pcap_datalink_;
    pcap_setfilter_t   pcap_setfilter_;
    pcap_compile_t     pcap_compile_;
    pcap_freecode_t    pcap_freecode_;
    pcap_freealldevs_t pcap_freealldevs_;
    pcap_close_t       pcap_close_;
    pcap_next_ex_t     pcap_next_ex_;
    pcap_geterr_t      pcap_geterr_;
};

}  // namespace libobsensor

#endif