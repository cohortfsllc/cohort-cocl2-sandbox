/*
 * Copyright (c) 2015 CohortFS LLC.
 */

#include <stdio.h>
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

#define ACCEPT_ON_BOOTSTRAP_SOCKET 0


int irt_cocl2_test(int a, int b, int* c) {
    printf("in irt_cocl2_test with params %d and %d\n",
           a, b);
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


int sendString(int socket, char* str) {
    struct NaClAbiNaClImcMsgHdr msg_hdr;
    struct NaClAbiNaClImcMsgIoVec msg_iov;

    msg_iov.base = str;
    msg_iov.length = 1 + strlen(str);

    msg_hdr.iov = &msg_iov;
    msg_hdr.iov_length = 1;
    msg_hdr.descv = NULL;
    msg_hdr.desc_length = 0;
    msg_hdr.flags = 0;
    
    return NACL_SYSCALL(imc_sendmsg)(socket, &msg_hdr, 0);
}


/*
 * Returns 0 on success, negative value on failure.
 *
 * NOTE: the call to imc_connect can block under windows. See:
 * http://code.google.com/p/nativeclient/issues/detail?id=692
 */
int irt_cocl2_init(int bootstrap_socket_addr) {
    printf("in irt_cocl2_init with socket %d\n", bootstrap_socket_addr);
    int socket = -1;

#if ACCEPT_ON_BOOTSTRAP_SOCKET
    socket = NACL_SYSCALL(imc_connect)(bootstrap_socket_addr);
    if (socket < 0) {
        perror("failed call to imc_connect");
        printf("Call to imc_connect returned %d\n", socket);
        return socket;
    } else {
        printf("imc_connect successfully returned %d\n", socket);
    }
#else
    printf("client using bootstrap socket addr to send message to directly\n");
    socket = bootstrap_socket_addr;
#endif

    int length_sent;

    length_sent = sendString(socket, "Hello, Chance!!!");
    printf("imc_sendmsg returned %d\n", length_sent);

    length_sent = sendString(socket, "Testing some more!!!");
    printf("imc_sendmsg returned %d\n", length_sent);

#if ACCEPT_ON_BOOTSTRAP_SOCKET
    // try to close no matter what (whether sendmsg succeeded or failed)
    int close_rv = NACL_SYSCALL(close)(socket);
    printf("close returned %d\n", close_rv);
#endif

    if (length_sent < 0) {
        return length_sent;
    }
#if ACCEPT_ON_BOOTSTRAP_SOCKET
    else if (close_rv < 0) {
        return close_rv;
    }
#endif
    else {
        return 0;
    }
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
