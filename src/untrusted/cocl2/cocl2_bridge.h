/*
 * Copyright (c) 2015 CohortFS LLC. All rights reserved.
 */
#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_COCL2_BRIDGE_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_COCL2_BRIDGE_H_

// duplicated from nacl_desc_base.h

struct NaClInternalRealHeaderCoCl2 {
    uint32_t xfer_protocol_version;
    uint32_t descriptor_data_bytes;
};


typedef struct {
    struct NaClInternalRealHeaderCoCl2 h;
    char pad[8]; // must pad to multiple of 16 bytes
} NaClInternalHeaderCoCl2;


#define NACL_HANDLE_TRANSFER_PROTOCOL_COCL2 0xd3c0de01


struct CoCl2RealHeader {
    uint32_t xfer_protocol_version;
};

typedef struct {
    struct CoCl2RealHeader h;
    char pad[12]; // must pad to multiple of 16 bytes
} CoCl2Header;


#define COCL2_PROTOCOL 0xc0404745


static inline NaClInternalHeaderCoCl2* getNaClHeader() {
    static NaClInternalHeaderCoCl2 * header = NULL;
    if (!header) {
        header =
            (NaClInternalHeaderCoCl2 *) calloc(sizeof(NaClInternalHeaderCoCl2),
                                               1);
        header->h.xfer_protocol_version = NACL_HANDLE_TRANSFER_PROTOCOL_COCL2;
        header->h.descriptor_data_bytes = 0;
    }
    return header;
}

static inline CoCl2Header* getCoCl2Header() {
    static CoCl2Header* header = NULL;
    if (!header) {
        header = (CoCl2Header*) calloc(sizeof(CoCl2Header), 1);
        header->h.xfer_protocol_version = COCL2_PROTOCOL;
    }
    return header;
}

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_IRT_COCL2_BRIDGE_H_ */
