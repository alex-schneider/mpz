
/**
 * Copyright (c) Alex Schneider
 */

#ifndef _MEMPOOLZ_MUTEX_H_INCLUDE
#define _MEMPOOLZ_MUTEX_H_INCLUDE

/* ==================================================================================================== */

#include "mpz_core.h"

/* ==================================================================================================== */

#ifdef MPZ_ENABLE_THREAD_SAFETY
typedef pthread_mutex_t  mpz_mutex_t;
#endif /* MPZ_ENABLE_THREAD_SAFETY */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define mpz_mutex_init(m, a) ( \
	(mpz_int_t)pthread_mutex_init((m), (a)) \
)

#define mpz_mutex_lock(m) ( \
	(mpz_int_t)pthread_mutex_lock((m)) \
)

#define mpz_mutex_unlock(m) ( \
	(mpz_int_t)pthread_mutex_unlock((m)) \
)

#define mpz_mutex_destroy(m) ( \
	(mpz_int_t)pthread_mutex_destroy((m)) \
)

/* ==================================================================================================== */

#endif /* _SERVERZ_MUTEX_H_INCLUDE */
