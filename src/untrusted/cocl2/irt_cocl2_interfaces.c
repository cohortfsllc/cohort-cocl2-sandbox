/*
 * Copyright (c) 2015 CohortFS LLC.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/public/irt_core.h"
#include "native_client/src/public/imc_types.h"
#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/pthread/pthread.h"
#include "native_client/src/untrusted/cocl2/irt_cocl2_interfaces.h"
#include "native_client/src/untrusted/cocl2/irt_cocl2.h"
#include "native_client/src/untrusted/cocl2/cocl2_bridge.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
// #include "native_client/src/include/nacl_compiler_annotations.h"
// #include "native_client/src/include/nacl_macros.h"
// #include "native_client/src/trusted/service_runtime/include/sys/unistd.h"
// #include "native_client/src/untrusted/cocl2/irt_cocl2.h"


pthread_mutex_t debug_mx;

bool debug_on = true;

#define DEBUG(format, ...) \
    do {                                                                \
    pthread_mutex_lock(&debug_mx);                                      \
    if (debug_on) {                                                     \
    fprintf(stderr, format, ##__VA_ARGS__ );                            \
    }                                                                   \
    pthread_mutex_unlock(&debug_mx);                                    \
    } while(0)


#define INFO(format, ...) DEBUG("INFO: " format "\n", ##__VA_ARGS__ )
#define ERROR(format, ...) DEBUG("ERROR: " format "\n", ##__VA_ARGS__ )


// since threads must have their own fixed-sized stack, and since NACL
// reserves the right to populate the top of the stack, this is some
// extra space we'll allocate for NACL to use.
const int additional_stack_addrs = 16;


#define ACCEPT_THREAD_STACK_SIZE (128 * 1024) // 128K

typedef struct {
    int socket_fd;
    char* algorithm_name;
    int stack_size_hint;
    struct cocl2_interface func_iface;
} accept_call_data;

// accept_call_data accept_thread_data;


typedef struct {
    int socket_fd;
    char* algorithm_name;
    struct cocl2_interface func_iface;
    char* buffer;
    int buffer_len;
    int sent_fd; // -1 for no fd, otherwise fd
} handle_call_data;


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
    struct NaClAbiNaClImcMsgIoVec msg_iov;
    msg_iov.base = buffer;
    msg_iov.length = buffer_len;

    struct NaClAbiNaClImcMsgHdr msg_hdr;
    msg_hdr.iov = &msg_iov;
    msg_hdr.iov_length = 1;
    msg_hdr.descv = handles;
    msg_hdr.desc_length = *handle_count;
    msg_hdr.flags = 0;

    int recv_len = imc_recvmsg(socket, &msg_hdr, 0);
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
    struct NaClAbiNaClImcMsgIoVec msg_iov[2];

    msg_iov[0].base = (void*) getCoCl2Header();
    msg_iov[0].length = sizeof(CoCl2Header);

    msg_iov[1].base = buffer;
    msg_iov[1].length = buffer_len;

    msg_hdr.iov = msg_iov;
    msg_hdr.iov_length = 2;
    msg_hdr.descv = handles;
    msg_hdr.desc_length = handle_count;
    msg_hdr.flags = 0;

    int rv = imc_sendmsg(socket, &msg_hdr, 0);
    // non-negative is success and length of data sent
    if (rv >= 0) {
        rv -= sizeof(CoCl2Header);
    }
    return rv;
}


int send_str(int socket, char* str, int handles[], int handle_count) {
    return send_buff(socket,
                     str, 1 + strlen(str),
                     handles, handle_count);
}


struct handle_request_thread_data {
    int* stack_flag;
};

/*
 * Must free args_temp to prevent memory leak.
 */
void* handle_request_thread(void* args_temp) {
    handle_call_data* args = (handle_call_data*) args_temp;

    INFO("In handle_request_thread for algorithm \"%s\"",
         args->buffer);

    char* buffer = "RECEIVED";
    int buffer_len = 1 + strlen(buffer);
    int sent_len = send_buff(args->socket_fd,
                             buffer, buffer_len,
                             NULL, 0);

    INFO("handle_request_thread sent %d, expected %d",
         sent_len, buffer_len);

    goto cleanup;

cleanup:
    INFO("handle request thread exiting");
    free(args_temp);
    return NULL;
}


// TODO: allow a shutdown message to be sent through socket
void* accept_thread(void* args_temp) {
    accept_call_data* args = (accept_call_data*) args_temp;

    char buffer[1024];
    int fd_len = NACL_ABI_IMC_USER_DESC_MAX;
    int fds[NACL_ABI_IMC_USER_DESC_MAX];

    while(1) {
        int recv_len = recv_buff(args->socket_fd,
                                 buffer, 1024,
                                 fds, &fd_len);
        INFO("received %d bytes", recv_len);

        if (recv_len < 0) {
            perror("recv_buff failed");
            continue;
        }

        handle_call_data* request_data =
            (handle_call_data *) calloc(sizeof(handle_call_data), 1);
        request_data->socket_fd = args->socket_fd;
        request_data->algorithm_name = args->algorithm_name;
        request_data->func_iface = args->func_iface;

        request_data->buffer_len = recv_len;
        request_data->buffer = (char *) calloc(recv_len, 1);
        memcpy(request_data->buffer, buffer, recv_len);

        request_data->sent_fd = fd_len > 0 ? fds[0] : -1;

        pthread_attr_t thread_attr;
        ASSERT(!pthread_attr_init(&thread_attr));
        ASSERT(!pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED));
        ASSERT(!pthread_attr_setscope(&thread_attr, PTHREAD_SCOPE_SYSTEM));
        ASSERT(!pthread_attr_setstacksize(&thread_attr, args->stack_size_hint));

        pthread_t thread_id;
        int rv = pthread_create(&thread_id,
                                &thread_attr,
                                handle_request_thread,
                                &request_data);
        if (rv) ERROR("pthread create resulted in %d", rv);

        ASSERT(!pthread_attr_destroy(&thread_attr));
    } // while(1)

// cleanup:

    INFO("accept thread exiting");
    return NULL;
} // accept thread


/*
 * Returns 0 on success, negative value on failure.
 *
 * NOTE: the call to imc_connect can block under windows. See:
 * http://code.google.com/p/nativeclient/issues/detail?id=692
 */
int irt_cocl2_init(const int bootstrap_socket_addr,
                   const char* algorithm_name,
                   const int stack_size_hint,
                   const struct cocl2_interface* func_iface) {
    int rv;

    rv = pthread_mutex_init(&debug_mx, NULL);
    if (rv) {
        fprintf(stderr, "ERROR: call to pthread_mutex_init failed.\n");
        perror("call to pthread_mutex_init");
        return rv;
    }

    INFO("in irt_cocl2_init with socket %d", bootstrap_socket_addr);

    int socket_pair[2];

    rv = imc_socketpair(socket_pair);
    if (rv) {
        ERROR("call to imc_makeboundsock failed: %d\n", rv);
        perror("call to makeboundsock");
        return rv;
    }

    const int my_socket = socket_pair[0];
    int their_socket = socket_pair[1];

    accept_call_data* accept_thread_data =
        (accept_call_data*) calloc(sizeof(accept_call_data), 1);

    accept_thread_data->socket_fd = my_socket;
    const int algorithm_name_len = 1+strlen(algorithm_name);
    accept_thread_data->algorithm_name = malloc(algorithm_name_len);
    strncpy(accept_thread_data->algorithm_name,
            algorithm_name,
            algorithm_name_len);
    accept_thread_data->func_iface = *func_iface;

    pthread_attr_t thread_attr;
    ASSERT(!pthread_attr_init(&thread_attr));
    ASSERT(!pthread_attr_setdetachstate(&thread_attr,
                                        PTHREAD_CREATE_JOINABLE));
    ASSERT(!pthread_attr_setscope(&thread_attr,
                                  PTHREAD_SCOPE_SYSTEM));
    ASSERT(!pthread_attr_setstacksize(&thread_attr,
                                      ACCEPT_THREAD_STACK_SIZE));

    pthread_t thread_id;
    rv = pthread_create(&thread_id,
                        &thread_attr,
                        accept_thread,
                        accept_thread_data);
    if (rv) {
        ERROR("calling pthread_create: %d", rv);
        close(my_socket);
        close(their_socket);
        return rv;
    }

    ASSERT(!pthread_attr_destroy(&thread_attr));

    char* verb = "REGISTER";
    char* message;
    int message_size = strlen(verb) + 1 + algorithm_name_len;
    message = (char *) calloc(message_size, 1);
    if (NULL == message) {
        ERROR("in allocating memory for REGISTER message");
        close(my_socket);
        close(their_socket);
        return -1;
    }

    sprintf(message, "%s%c%s", verb, '\0', algorithm_name);

    INFO("about to call send_buff from cocl2_init");

    int length_sent = send_buff(bootstrap_socket_addr,
                                message, message_size,
                                &their_socket, 1);

    INFO("send_buff returned %d, expected %d",
         length_sent,
         message_size);

    free(message);
    if (length_sent < 0) {
        ERROR("Calling sendString");
        return length_sent;
    }

    INFO("waiting for accepting thread to exit");

    void* return_location;
    rv = pthread_join(thread_id, &return_location);

    free(accept_thread_data);

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
