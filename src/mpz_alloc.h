
/**
 * Copyright (c) Alex Schneider
 */

#ifndef _MEMPOOLZ_API_H_INCLUDE
#define _MEMPOOLZ_API_H_INCLUDE

#include "mpz_core.h"

/* ==================================================================================================== */

typedef struct _mpz_pool_s  mpz_pool_t;
typedef struct _mpz_slab_s  mpz_slab_t;
typedef struct _mpz_slot_s  mpz_slot_t;

struct _mpz_slot_s
{
	mpz_void_t  *data;
	mpz_slot_t  *next;
};

struct _mpz_slab_s
{
	mpz_slab_t  *prev;
	mpz_slab_t  *next;
};

struct _mpz_pool_s
{
	mpz_slot_t  *bins[MPZ_BINS];
	mpz_slab_t  *slabs;

#ifdef MPZ_ENABLE_THREAD_SAFETY
	mpz_mutex_t  mutex;
#endif /* MPZ_ENABLE_THREAD_SAFETY */
};

/* ==================================================================================================== */

mpz_pool_t *mpz_pool_create(
	mpz_void_t
);

mpz_int_t mpz_pool_reset(
	mpz_pool_t *pool
);

mpz_int_t mpz_pool_destroy(
	mpz_pool_t *pool
);

mpz_void_t *mpz_pmalloc(
	mpz_pool_t *pool, mpz_csize_t size
);

mpz_void_t *mpz_pcalloc(
	mpz_pool_t *pool, mpz_csize_t size
);

mpz_int_t mpz_free(
	mpz_pool_t *pool, mpz_cvoid_t *data
);

/* ==================================================================================================== */

#endif /* _MEMPOOLZ_API_H_INCLUDE */
