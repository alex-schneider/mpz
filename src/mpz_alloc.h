
/**
 * Copyright (c) Alex Schneider
 */

#ifndef _MEMPOOLZ_API_H_INCLUDE
#define _MEMPOOLZ_API_H_INCLUDE

#include "mpz_core.h"

/* ==================================================================================================== */

mpz_pool_t *mpz_pool_create(
	mpz_void_t
);

mpz_void_t mpz_pool_reset(
	mpz_pool_t *pool
);

mpz_void_t mpz_pool_destroy(
	mpz_pool_t *pool
);

mpz_void_t *mpz_pmalloc(
	mpz_pool_t *pool, mpz_csize_t size
);

mpz_void_t *mpz_pcalloc(
	mpz_pool_t *pool, mpz_csize_t size
);

mpz_void_t mpz_free(
	mpz_pool_t *pool, mpz_cvoid_t *data
);

/* ==================================================================================================== */

#endif /* _MEMPOOLZ_API_H_INCLUDE */
