/*
 * Copyright (c) 2015 CohortFS LLC.
 */


#include "native_client/src/public/irt_core.h"
#include "native_client/src/untrusted/cocl2/irt_cocl2_interfaces.h"
#include "native_client/src/untrusted/cocl2/irt_cocl2.h"
// #include "native_client/src/include/nacl_compiler_annotations.h"
// #include "native_client/src/include/nacl_macros.h"
// #include "native_client/src/trusted/service_runtime/include/sys/unistd.h"
// #include "native_client/src/untrusted/cocl2/irt_cocl2.h"
// #include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

int irt_cocl2_test(int a, int b, int* c) {
    *c = a + b;
    return 0;
}

const struct nacl_irt_cocl2 nacl_irt_cocl2 = {
    irt_cocl2_test,
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
