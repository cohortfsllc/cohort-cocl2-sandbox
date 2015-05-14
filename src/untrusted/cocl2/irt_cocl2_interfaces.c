/*
 * Copyright (c) 2015 CohortFS LLC.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "native_client/src/public/irt_core.h"
#include "native_client/src/public/imc_types.h"
#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/pthread/pthread.h"
#include "native_client/src/untrusted/cocl2/irt_cocl2_interfaces.h"
#include "native_client/src/untrusted/cocl2/irt_cocl2.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
// #include "native_client/src/include/nacl_compiler_annotations.h"
// #include "native_client/src/include/nacl_macros.h"
// #include "native_client/src/trusted/service_runtime/include/sys/unistd.h"
// #include "native_client/src/untrusted/cocl2/irt_cocl2.h"


bool debug_on = true;

#define INFO(args...) if (debug_on) { fprintf(stderr, "INFO: " args); fputc('\n', stderr); fflush(stderr); }
#define ERROR(args...) if (debug_on) { fprintf(stderr, "ERROR: " args); fputc('\n', stderr); fflush(stderr); }


// since threads must have their own fixed-sized stack, and since NACL
// reserves the right to populate the top of the stack, this is some
// extra space we'll allocate for NACL to use.
const int additional_stack_addrs = 16;


#define ACCEPT_THREAD_STACK_SIZE 128 // 128 addresses
// int32_t accept_thread_stack_flag = 1;
typedef struct {
    int socket_fd;
    char* algorithm_name;
    int stack_size_hint;
    struct cocl2_interface interface;
} accept_thread_t;

accept_thread_t accept_thread_data;

int irt_cocl2_set_debug(bool value) {
    debug_on = value;
    return 0;
}


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


int recv_buff(int socket,
              void* buffer, int buffer_len,
              int handles[], int* handle_count) {
    struct NaClAbiNaClImcMsgHdr msg_hdr;
    struct NaClAbiNaClImcMsgIoVec msg_iov;

    msg_iov.base = buffer;
    msg_iov.length = buffer_len;

    msg_hdr.iov = &msg_iov;
    msg_hdr.iov_length = 1;
    msg_hdr.descv = handles;
    msg_hdr.desc_length = *handle_count;
    msg_hdr.flags = 0;

#if 0
    int recv_len = imc_recvmsg(socket, &msg_hdr, 0);
#else
    int recv_len = NACL_SYSCALL(imc_recvmsg)(socket, &msg_hdr, 0);
#endif
    if (recv_len > 0) {
        *handle_count = msg_hdr.desc_length;
    }

    INFO("imc_recvmsg received message of length %d plus %d file descriptors",
         recv_len, msg_hdr.desc_length);

    return recv_len;
}


int send_buff(int socket,
              void* buffer, int buffer_len,
              int handles[], int handle_count) {
    struct NaClAbiNaClImcMsgHdr msg_hdr;
    struct NaClAbiNaClImcMsgIoVec msg_iov;

    msg_iov.base = buffer;
    msg_iov.length = buffer_len;

    msg_hdr.iov = &msg_iov;
    msg_hdr.iov_length = 1;
    msg_hdr.descv = handles;
    msg_hdr.desc_length = handle_count;
    msg_hdr.flags = 0;

#if 0    
    return imc_sendmsg(socket, &msg_hdr, 0);
#else
    return NACL_SYSCALL(imc_sendmsg)(socket, &msg_hdr, 0);
#endif
}


int send_str(int socket, char* str, int handles[], int handle_count) {
    return send_buff(socket,
                     str, 1 + strlen(str),
                     handles, handle_count);
}


struct handle_request_thread_data {
    int* stack_flag;
};


void handle_request_thread(void) {
    struct handle_request_thread_data *thread_data =
        (struct handle_request_thread_data*) NACL_SYSCALL(tls_get)();
    if (!thread_data) {
        ERROR("call to tls_get failed");
        NACL_SYSCALL(thread_exit)(NULL);
        return;
    }

    goto cleanup;

cleanup:
    INFO("handle request thread exiting");
    NACL_SYSCALL(thread_exit)(thread_data->stack_flag);
}


void* accept_thread(void* arg_temp) {
    int rv;

    accept_thread_t* arg = (accept_thread_t*) arg_temp;

    char buffer[1024];
    int fd_len = 16;
    int fds[16];

    while(1) {
        int request_fd = imc_accept(arg->socket_fd);
        if (request_fd < 0) {
            ERROR("failure on imc_accept: %d", request_fd);
            continue;
        }


        int recv_len = recv_buff(request_fd,
                                 buffer, 1024,
                                 fds, &fd_len);
        INFO("received %d bytes", recv_len);

        char buffer2[1024];
        sprintf(buffer2, "RECEIVED: %s\n", buffer);

        int send_len = send_buff(request_fd,
                                 buffer2, 1024,
                                 NULL, 0);
        INFO("sent %d bytes", send_len);

#if 1        
        rv = close(request_fd);
#else
        rv = NACL_SYSCALL(close)(request_fd);
#endif
        if (rv) ERROR("close of socket resulted in %d", rv);

#if 0 // not yet
        void* handle_request_stack =
            malloc((additional_stack_addrs +
                    arg->stack_size_hint) *
                   sizeof(void*));
#endif
    } // while(1)

// cleanup:

    INFO("accept thread exiting");
    return NULL;
}


/*
 * Returns 0 on success, negative value on failure.
 *
 * NOTE: the call to imc_connect can block under windows. See:
 * http://code.google.com/p/nativeclient/issues/detail?id=692
 */
int irt_cocl2_init(const int bootstrap_socket_addr,
                   const char* algorithm_name,
                   const int stack_size_hint,
                   const struct cocl2_interface* interface) {
    int rv;

    INFO("in irt_cocl2_init with socket %d", bootstrap_socket_addr);

    int bound_pair[2];

    rv = imc_makeboundsock(bound_pair);
    if (rv) {
        ERROR("call to imc_makeboundsock failed: %d\n", rv);
        perror("call to makeboundsock");
        return rv;
    }

    int socket_addr = bound_pair[1];

    accept_thread_data.socket_fd = bound_pair[0];
    const int algorithm_name_len = 1+strlen(algorithm_name);
    accept_thread_data.algorithm_name = malloc(algorithm_name_len);
    strncpy(accept_thread_data.algorithm_name,
            algorithm_name,
            algorithm_name_len);
    accept_thread_data.interface = *interface;

    pthread_t thread_id;

    pthread_attr_t thread_attr;
    thread_attr.joinable = 1;
    thread_attr.stacksize = ACCEPT_THREAD_STACK_SIZE * sizeof(void*);


    rv = pthread_create(&thread_id,
                        &thread_attr,
                        accept_thread,
                        &accept_thread_data);
    if (rv) {
        ERROR("calling pthread_create: %d", rv);
        close(bound_pair[0]);
        return rv;
    }

    char* verb = "REGISTER";
    char* message;
    int message_size = strlen(verb) + 1 + algorithm_name_len;
    message = (char *) malloc(message_size);
    if (NULL == message) {
        ERROR("in allocating memory for REGISTER message");
        close(bound_pair[0]);
        return -1;
    }

    sprintf(message, "%s%c%s", verb, '\0', algorithm_name);

    int length_sent = send_buff(bootstrap_socket_addr,
                                message, message_size,
                                &socket_addr, 1);

    INFO("imc_sendmsg returned %d, expected %d",
         length_sent,
         message_size);

    free(message);

    if (length_sent < 0) {
        ERROR("Calling sendString");
        return length_sent;
    }

    return 0;
}


const struct nacl_irt_cocl2 nacl_irt_cocl2 = {
    irt_cocl2_test,
    irt_cocl2_getpid,
    irt_cocl2_gettod,
    irt_cocl2_init,
    irt_cocl2_set_debug,
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
