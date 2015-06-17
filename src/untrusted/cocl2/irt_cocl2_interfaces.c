/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
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


#define ACCEPT_BUFFER_LEN    1024
#define MAX_OSDS_REQUESTABLE   64
#define MAX_SEND_IOVEC          8

bool debug_on = true;
char* debug_header = "I";

#define DEBUG(format, ...) \
    do {                                                                \
    if (debug_on) {                                                     \
        fprintf(stderr, format, debug_header, ##__VA_ARGS__ );          \
    }                                                                   \
    } while(0)


#define INFO(format, ...) DEBUG("INFO %s: " format "\n", ##__VA_ARGS__ )
#define ERROR(format, ...) DEBUG("ERROR %s: " format "\n", ##__VA_ARGS__ )


void ignore(void* ignored) {
    // empty
}

#define IGNORE(v) while(0) { ignore(&v); }


// since threads must have their own fixed-sized stack, and since NACL
// reserves the right to populate the top of the stack, this is some
// extra space we'll allocate for NACL to use.
const int additional_stack_addrs = 16;


#define ACCEPT_THREAD_STACK_SIZE (128 * 1024) // 128K

typedef struct {
    int socket_fd;
    char* algorithm_name; // thread must free
    int stack_size_hint;
    struct cocl2_interface func_iface;
} accept_call_data;

// accept_call_data accept_thread_data;


typedef struct {
    int socket_fd;
    char* algorithm_name; // thread may not free
    struct cocl2_interface* func_iface;
    char* buffer;
    int buffer_len;
} handle_call_data;


void print_buffer(void* buffer, int buffer_len, int skip) {
    char* b = (char*) buffer;
    for (int i = skip; i < skip + buffer_len; ++i) {
        printf("%d: %3hhu %2hhx %c\n", i, (unsigned char) b[i], b[i], b[i]);
    }
}


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

    return recv_len;
}


int recv_cocl2_buff(int socket,
                    void* buffer, int buffer_len,
                    int handles[], int* handle_count,
                    int* bytes_to_skip) {
    *bytes_to_skip = 0;

    int length = recv_buff(socket,
                           buffer, buffer_len,
                           handles, handle_count);
    if (length < 0) {
        return length;
    }

    CoCl2Header* header = (CoCl2Header*) buffer;
    if (header->h.xfer_protocol_version != COCL2_PROTOCOL) {
        errno = EPROTONOSUPPORT;
        return -errno;
    }

    *bytes_to_skip = sizeof(CoCl2Header);
    return length - *bytes_to_skip;
}


int send_cocl2_buff_old(int socket,
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


int send_cocl2_buff(int socket,
                    int handles[], int handle_count,
                    ...) {
    struct NaClAbiNaClImcMsgHdr msg_hdr;
    struct NaClAbiNaClImcMsgIoVec msg_iov[1 + MAX_SEND_IOVEC];

    msg_iov[0].base = (void*) getCoCl2Header();
    msg_iov[0].length = sizeof(CoCl2Header);

    va_list access;
    va_start(access, handle_count);
    int count;
    for (count = 0; ; count++) {
        char* buffer = va_arg(access, char*);
        if (!buffer) {
            break;
        }

        if (count >= MAX_SEND_IOVEC) {
            // TODO handle better
            ERROR("tried to send too many buffers");
            --count;
            break;
        }

        int buffer_len = va_arg(access, int);

        msg_iov[1 + count].base = buffer;
        msg_iov[1 + count].length = buffer_len;
    }
    va_end(access);

    msg_hdr.iov = msg_iov;
    msg_hdr.iov_length = count + 1;
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
    return send_cocl2_buff(socket,
                           handles, handle_count,
                           str, 1 + strlen(str), NULL);
}


/*
 * Must free args_temp to prevent memory leak.
 */
void* handle_call_thread(void* args_temp) {
    handle_call_data* args = (handle_call_data*) args_temp;

    OpCallParams* params_min = (OpCallParams*) args->buffer;

    if (params_min->func_id != FUNC_PLACEMENT) {
        ERROR("unknown function %04x", params_min->func_id);
        goto error;
    }

    if (params_min->part != -1) {
        ERROR("multi-part messages not implemented yet");
        goto error;
    }

    OpPlacementCallParams* params = (OpPlacementCallParams*) args->buffer;

    if (params->osds_requested > MAX_OSDS_REQUESTABLE) {
        ERROR("too many osds requested %d > %d",
              params->osds_requested,
              MAX_OSDS_REQUESTABLE);
        goto error;
    }

    uint32_t osd_list[MAX_OSDS_REQUESTABLE];
    char* object_name = args->buffer + sizeof(OpPlacementCallParams);

    INFO("about to call, uuid is %p, object_name is %p",
         params->uuid,
         object_name);

    int rv = args->func_iface->cocl2_compute_osds(params->uuid,
                                                  object_name,
                                                  osd_list,
                                                  params->osds_requested);
    if (rv) {
        ERROR("compute osds function returned an error");
        goto error;
    }

    // SUCCESS
    INFO("successfully computed osds");
    OpReturnParams ret_params;
    ret_params.epoch = params->call_params.epoch;
    ret_params.part = -1;

    int sent_len;

    // send three things back -- RETURN op code, return params, and the list
    // of osds
    sent_len =
        send_cocl2_buff(args->socket_fd,
                        NULL, 0,
                        OP_RETURN, OP_SIZE,
                        &ret_params, sizeof(ret_params),
                        osd_list, params->osds_requested * sizeof(osd_list[0]),
                        NULL);

    if (sent_len < 0) {
        ERROR("could not send return to caller %d", sent_len);
    }
    INFO("successfully sent computed osds back to outside");

    goto cleanup;

error:; // ; required since next line is a declaration

    OpErrorParams error_params;
    error_params.ret_params.epoch = params_min->epoch;
    error_params.ret_params.part = -1;
    
    error_params.error_code = -17; // TODO have various
    static const char* error_message = "Unspecified Error";
    int error_message_size = 1 + strlen(error_message);

    sent_len =
        send_cocl2_buff(args->socket_fd,
                        NULL, 0,
                        OP_ERROR, OP_SIZE,
                        &error_params, sizeof(error_params),
                        error_message, error_message_size,
                        NULL);

    if (sent_len < 0) {
        ERROR("could not send error to caller %d", sent_len);
    }

cleanup:
    INFO("handle call thread exiting");
    free(args_temp);
    return NULL;
}


int launch_call_thread(accept_call_data* accept_args,
                       char* buffer,
                       int buffer_len) {
    handle_call_data* call_data =
        (handle_call_data *) calloc(sizeof(handle_call_data), 1);
    if (!call_data) {
        return -1;
    }

    call_data->socket_fd = accept_args->socket_fd;
    call_data->algorithm_name = accept_args->algorithm_name;
    call_data->func_iface = &accept_args->func_iface;

    call_data->buffer_len = buffer_len;
    call_data->buffer = (char *) calloc(buffer_len, 1);
    memcpy(call_data->buffer, buffer, buffer_len);

    pthread_attr_t thread_attr;
    ASSERT(!pthread_attr_init(&thread_attr));
    ASSERT(!pthread_attr_setdetachstate(&thread_attr,
                                        PTHREAD_CREATE_DETACHED));
    ASSERT(!pthread_attr_setscope(&thread_attr,
                                  PTHREAD_SCOPE_SYSTEM));
    ASSERT(!pthread_attr_setstacksize(&thread_attr,
                                      accept_args->stack_size_hint));
    INFO("handle_call stack size is 0x%x",
         accept_args->stack_size_hint);

    pthread_t thread_id;
    int rv = pthread_create(&thread_id,
                            &thread_attr,
                            handle_call_thread,
                            call_data);
    if (rv) {
        return rv;
    }

    rv = pthread_attr_destroy(&thread_attr);

    return rv;
}


// TODO: allow a shutdown message to be sent through socket
void* accept_thread(void* args_temp) {
    accept_call_data* args = (accept_call_data*) args_temp;


    char buffer[ACCEPT_BUFFER_LEN];
    int fd_len = NACL_ABI_IMC_USER_DESC_MAX;
    int fds[NACL_ABI_IMC_USER_DESC_MAX];

    while (1) {
        int bytes_to_skip;
        INFO("algorithm %s waiting for messages", args->algorithm_name);
        int recv_len = recv_cocl2_buff(args->socket_fd,
                                       buffer, ACCEPT_BUFFER_LEN,
                                       fds, &fd_len,
                                       &bytes_to_skip);
        if (recv_len < 0) {
            perror("accept_thread got bad call");
            continue;
        }

        char* buffer2 = buffer + bytes_to_skip;

        if (OPS_EQUAL(OP_CALL, buffer2)) {
            INFO("received OP CALL");
            int rv = launch_call_thread(args,
                                        buffer2 + OP_SIZE,
                                        recv_len - OP_SIZE);
            if (rv) {
                perror("accept_thread could not launch call thread");
                continue;
            }
        } else if (OPS_EQUAL(OP_SHUTDOWN, buffer2)) {
            INFO("received OP SHUTDOWN");
            break;
        } else if (OPS_EQUAL(OP_SHARE_DATA, buffer2)) {
            INFO("OP SHARE DATA not implemented yet");
        } else {
            ERROR("OP \"%s\" not recognized", buffer2);
        }
    } // while(1)

    INFO("accept thread exiting");
    free(args_temp);
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
    accept_thread_data->stack_size_hint = stack_size_hint;
    const int algorithm_name_size = 1 + strlen(algorithm_name);
    accept_thread_data->algorithm_name = malloc(algorithm_name_size);
    strncpy(accept_thread_data->algorithm_name,
            algorithm_name,
            algorithm_name_size);
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

    int message_size = OP_SIZE + algorithm_name_size;
    char* message = (char *) calloc(message_size, 1);
    if (NULL == message) {
        ERROR("in allocating memory for REGISTER message");
        close(my_socket);
        close(their_socket);
        return -1;
    }

    sprintf(message, "%s%c%s", OP_REGISTER, '\0', algorithm_name);

    int length_sent = send_cocl2_buff(bootstrap_socket_addr,
                                      &their_socket, 1,
                                      message, message_size,
                                      NULL);

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
