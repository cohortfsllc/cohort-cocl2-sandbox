/*
 * Copyright 2015 CohortFS LLC, all rights reserved.
 */


#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_COCL2_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_COCL2_H_

#include <stddef.h>
#include <sys/types.h>


/*
 * Since the NACL build system does not have access to uuid/uuid.h, we'll
 * use an opaque internally, which matches the size of a UUID.
 */
typedef unsigned char uuid_opaque[16];


/* Use relative path so that irt_cocl2.h can be installed as a system header. */
// #include "irt.h"


#if defined(__cplusplus)
extern "C" {
#endif


#define COCL2_INTERFACE_v0_1 "cocl2-interface-0.1"
struct cocl2_interface {
    int (*cocl2_shutdown)(void);
    int (*cocl2_alloc_shared_mem)(unsigned size,
                                  int* mem_handle);
    int (*cocl2_free_shared_mem)(int handle);
    int (*cocl2_compute_osds)(const uuid_opaque volume_uuid,
                              const char* obj_name,
                              uint32_t* osd_list,
                              const int osd_count);
};



/*
 * CoCl2
 */

#define NACL_IRT_COCL2_v0_1 "nacl-irt-cocl2-0.1"
struct nacl_irt_cocl2 {
    int (*cocl2_test)(int a, int b, int* c);
    int (*cocl2_getpid)(int* pid);
    int (*cocl2_gettod)(struct timeval* tod);
    int (*cocl2_init)(const int bootstrap_socket_addr,
                      const char* algorithm_name,
                      const int alg_stack_size_hint,
                      const struct cocl2_interface* entry_points);
    int (*cocl2_set_debug)(bool setting);
};


#if defined(__cplusplus)
}
#endif

#endif /* NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_COCL2_H_ */
