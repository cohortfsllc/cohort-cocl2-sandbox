/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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

/*
 * General notes about IRT interfaces:
 *
 * All functions in IRT vectors return an int, which is zero for success
 * or a (positive) errno code for errors.  Any values are delivered via
 * result parameters.  The only exceptions exit/thread_exit, which can
 * never return, and tls_get, which can never fail.
 *
 * Some of the IRT interfaces below are disabled under PNaCl because
 * they are deprecated or not portable.  The list of IRT interfaces
 * that are allowed under PNaCl can be found in the Chromium repo in
 * ppapi/native_client/src/untrusted/pnacl_irt_shim/shim_ppapi.c.
 *
 * Interfaces with "-dev" in the query string are not
 * permanently-supported stable interfaces.  They might be removed in
 * future versions of Chromium.
 */


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
