/*
 * Copyright (c) 2015 CohortFS LLC. All rights reserved.
 */
#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_COCL2_INTERFACES_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_COCL2_INTERFACES_H_

#include <stddef.h>

extern const struct nacl_irt_cocl2 nacl_irt_cocl2;

size_t nacl_irt_query_cocl2(const char *interface_ident,
                            void *table, size_t tablesize);

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_COCL2_INTERFACES_H_ */
