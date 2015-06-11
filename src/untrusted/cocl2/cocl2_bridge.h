/*
 * Copyright (c) 2015 CohortFS LLC. All rights reserved.
 */
#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_COCL2_BRIDGE_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_COCL2_BRIDGE_H_


// number of bytes in OPERATIONS; we're using four so they can be
// compared as four-byte unsigned ints
#define OP_LEN       4
#define OP_SIZE      (1 + OP_LEN) // null-terminating char

// outside->sandbox
#define OP_SHUTDOWN   "SHUT"
#define OP_CALL       "CALL"
#define OP_SHARE_DATA "SHAR"

// sandbox->outside
#define OP_REGISTER   "REGI"
#define OP_RETURN     "RETU"
#define OP_ERROR      "ERRO"

#define FUNC_PLACEMENT 0x97AC


typedef struct {
    uint32_t epoch;   // a unique value per call
    int32_t  part;    // e.g., 1, 2, -3 (last part is negative)
    uint32_t func_id; // function being called
} OpCallParams;


typedef struct {
    OpCallParams call_params;
    uint32_t osds_requested;
    uuid_opaque uuid;
    // char object_name; // first char of object name; null terminated string
} OpPlacementCallParams;    


typedef struct {
    uint32_t epoch;   // a unique value per call
    int32_t  part;    // e.g., 1, 2, -3 (last part is negative)
} OpReturnParams;


typedef struct {
    OpReturnParams ret_params;
    int32_t error_code;
    // char error_msg; // first char of null-terminated message
} OpErrorParams;


// compare ops as 32-bit (4 byte) unsigned ints
#define OPS_EQUAL(op1, op2) ((* (uint32_t *) (op1)) == (* (uint32_t *) (op2)))


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
