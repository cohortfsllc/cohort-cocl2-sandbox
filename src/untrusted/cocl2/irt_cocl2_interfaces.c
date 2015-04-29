/*
 * Copyright (c) 2015 CohortFS LLC.
 */

#include <string.h>

#include "native_client/src/public/irt_core.h"
#include "native_client/src/public/imc_types.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/cocl2/irt_cocl2_interfaces.h"
#include "native_client/src/untrusted/cocl2/irt_cocl2.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
// #include "native_client/src/include/nacl_compiler_annotations.h"
// #include "native_client/src/include/nacl_macros.h"
// #include "native_client/src/trusted/service_runtime/include/sys/unistd.h"
// #include "native_client/src/untrusted/cocl2/irt_cocl2.h"


int irt_cocl2_test(int a, int b, int* c) {
    *c = a + b;
    return 0;
}


int irt_cocl2_getpid(int* pid) {
    int rv = NACL_SYSCALL(getpid)();
    if (rv < 0)
        return -rv;
    *pid = rv;
    return 0;
}


int irt_cocl2_gettod(struct timeval* tod) {
    return -NACL_SYSCALL(gettimeofday)(tod);
}

int irt_cocl2_init(int bootstrap_socket_addr) {
    int socket = NACL_SYSCALL(imc_connect)(bootstrap_socket_addr);
    if (socket < 0) return socket;

    struct NaClAbiNaClImcMsgHdr msg_hdr;

    struct NaClAbiNaClImcMsgIoVec msg_iov;

    char* message = "Hello, Chance!";

    msg_iov.base = message;
    msg_iov.length = 1 + strlen(message);

    msg_hdr.iov = &msg_iov;
    msg_hdr.iov_length = 1;
    msg_hdr.descv = NULL;
    msg_hdr.desc_length = 0;
    msg_hdr.flags = 0;
    
    int send_rv = NACL_SYSCALL(imc_sendmsg)(socket, &msg_hdr, 0);
    int close_rv = NACL_SYSCALL(close)(socket);

    return -(send_rv || close_rv); 
}


const struct nacl_irt_cocl2 nacl_irt_cocl2 = {
    irt_cocl2_test,
    irt_cocl2_getpid,
    irt_cocl2_gettod,
    irt_cocl2_init,
};


static const struct nacl_irt_interface irt_cocl2_interfaces[] = {
    { NACL_IRT_COCL2_v0_1, &nacl_irt_cocl2, sizeof(nacl_irt_cocl2), NULL },
};


size_t nacl_irt_query_cocl2(const char *interface_ident,
                            void *table, size_t tablesize) {
    return nacl_irt_query_list(interface_ident, table, tablesize,
                               irt_cocl2_interfaces,
                               sizeof(irt_cocl2_interfaces));
}
