
/**
 * Copyright (c) Alex Schneider
 */

#include "mpz_core.h"

/* ==================================================================================================== */
/* PRIVATE STUFF: DECLARATIONS */

#define mpz_memalign(a, s)   ({ mpz_void_t *p; (0 != posix_memalign(&p, (a), (s)) ? NULL : p); })
#define mpz_memzero(p, n)    ({ memset((mpz_void_t *)(p), 0, (n)); })
#define mpz_align(s, a)      (((s) + ((a) - 1)) & ~((a) - 1))

#define MPZ_ALLOC_ALIGNMENT  (2 * sizeof(mpz_void_t *))

#define MPZ_CHECK_VOID(p)    ({ if (NULL == (p)) return; })
#define MPZ_CHECK_NULL(p)    ({ if (NULL == (p)) return NULL; })

#define MPZ_SLOT_SIZE        (sizeof(mpz_slot_t *) + sizeof(mpz_uint32_t) * 2)
#define MPZ_SLAB_SIZE        (mpz_align(sizeof(mpz_slab_t), MPZ_ALLOC_ALIGNMENT))
#define MPZ_POOL_SIZE        (mpz_align(sizeof(mpz_pool_t), MPZ_ALLOC_ALIGNMENT))

#define MPZ_SLAB_TO_SLOT(s)  ((mpz_slot_t *)(((mpz_uchar_t *)(s)) + MPZ_SLAB_SIZE))
#define MPZ_SLOT_TO_SLAB(s)  ((mpz_slab_t *)(((mpz_uchar_t *)(s)) - MPZ_SLAB_SIZE))
#define MPZ_SLOT_TO_DATA(s)  ((mpz_void_t *)(((mpz_uchar_t *)(s)) + sizeof(mpz_uint32_t)))
#define MPZ_DATA_TO_SLOT(d)  ((mpz_slot_t *)(((mpz_uchar_t *)(d)) - sizeof(mpz_uint32_t)))

#define MPZ_SLOT_FLAG_USED   ((mpz_cuint32_t)(1 << 30))
#define MPZ_SLOT_FLAG_HUGE   ((mpz_cuint32_t)(1 << 31))

#define MPZ_POOL_MIN_ALLOC   (MPZ_SLOTS_ALIGNMENT)
#define MPZ_POOL_MAX_ALLOC   ((mpz_cuint32_t)((1 << 29) - 1))

#define MPZ_SLOT_READ_HEAD(s)       ( \
	(mpz_uint32_t *)(s) \
)
#define MPZ_SLOT_READ_SIZE(s)       ( \
	(mpz_uint32_t)((*MPZ_SLOT_READ_HEAD(s) << 2) >> 2) \
)

#ifdef MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS

#define MPZ_FOOTER_MARK  (0xFFFFFFFF)

#define MPZ_SLOT_GOTO_FOOT(sl, si)  ( \
	(mpz_uint32_t *)((mpz_uchar_t *)(sl) + sizeof(mpz_uint32_t) + (si)) \
)
#define MPZ_SLOT_READ_FOOT(s)       ( \
	(mpz_uint32_t *)MPZ_SLOT_GOTO_FOOT((s), MPZ_SLOT_READ_SIZE(s)) \
)

#endif /* MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static inline mpz_void_t _mpz_pool_gc(
	mpz_pool_t *pool, mpz_cuint_t soft
);

static inline mpz_void_t *_mpz_palloc(
	mpz_pool_t *pool, mpz_size_t size, mpz_cuint_t zeroize
);

static inline mpz_void_t *_mpz_slab_create(
	mpz_pool_t *pool, mpz_csize_t size
);

static inline mpz_void_t _mpz_slab_init(
	mpz_pool_t *pool, mpz_slab_t *slab, mpz_cuint32_t size
);

static inline mpz_void_t _mpz_slab_push(
	mpz_pool_t *pool, mpz_slab_t *slab
);

static inline mpz_void_t _mpz_slab_free(
	mpz_pool_t *pool, mpz_slot_t *slot
);

static inline mpz_void_t _mpz_slot_init(
	mpz_slot_t *slot, mpz_cuint32_t size, mpz_cuint32_t flags
);

/* ==================================================================================================== */
/* PUBLIC API */

mpz_pool_t *mpz_pool_create(
	mpz_void_t
) {
	mpz_pool_t *pool;
	mpz_uint_t  idx;

	MPZ_CHECK_NULL(pool = mpz_memalign(MPZ_ALLOC_ALIGNMENT, MPZ_POOL_SIZE));

	for (idx = 0; idx < MPZ_BINS; idx++) {
		pool->bins[idx] = NULL;
	}

	pool->slabs = NULL;

	return pool;
}

mpz_void_t mpz_pool_reset(
	mpz_pool_t *pool
) {
	_mpz_pool_gc(pool, 1);
}

mpz_void_t mpz_pool_destroy(
	mpz_pool_t *pool
) {
	_mpz_pool_gc(pool, 0);

	free(pool);
}

mpz_void_t *mpz_pmalloc(
	mpz_pool_t *pool, mpz_csize_t size
) {
	return _mpz_palloc(pool, size, 0);
}

mpz_void_t *mpz_pcalloc(
	mpz_pool_t *pool, mpz_csize_t size
) {
	return _mpz_palloc(pool, size, 1);
}

mpz_void_t mpz_free(
	mpz_pool_t *pool, mpz_cvoid_t *data
) {
	mpz_slot_t   *slot;
	mpz_uint32_t *head, size;
	mpz_uint_t    idx;

	MPZ_CHECK_VOID(pool);
	MPZ_CHECK_VOID(data);

	slot = MPZ_DATA_TO_SLOT(data);
	head = MPZ_SLOT_READ_HEAD(slot);

#ifdef MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS

	/* Check both "segmentation faults" and "double free" errors. */
	if ((MPZ_FOOTER_MARK != *MPZ_SLOT_READ_FOOT(slot)) || (!(*head & MPZ_SLOT_FLAG_USED))) {
		raise(SIGSEGV);
	}

#endif /* MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS */

	if (*head & MPZ_SLOT_FLAG_HUGE) {
		return _mpz_slab_free(pool, slot);
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	size = MPZ_SLOT_READ_SIZE(slot);

#ifdef MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS

	/* Remove the "used" mark. */
	_mpz_slot_init(slot, size, 0);

#endif /* MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS */

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Push the slot into the bins-array. */

	idx  = MPZ_BIN_IDX(size);

	slot->next = pool->bins[idx];

	pool->bins[idx] = slot;
}

/* ==================================================================================================== */
/* PRIVATE STUFF: DEFINITIONS */

static inline mpz_void_t _mpz_pool_gc(
	mpz_pool_t *pool, mpz_cuint_t soft
) {
	mpz_slab_t *slab, *next;
	mpz_slot_t *slot;
	mpz_uint_t  idx;

	MPZ_CHECK_VOID(pool);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	for (idx = 0; idx < MPZ_BINS; idx++) {
		pool->bins[idx] = NULL;
	}

	slab = pool->slabs;
	pool->slabs = NULL;

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	/**
	 * Slabs contain a huge slot are immediately destroyed. If "soft"
	 * is set to 0, all other slabs are immediately destroyed, too,
	 * otherwise the other slabs are reseted to theirs initial state
	 * to permitt the pool to be efficiently reused.
	*/

	while (NULL != slab) {
		next = slab->next;
		slot = MPZ_SLAB_TO_SLOT(slab);

		if (!soft || (*MPZ_SLOT_READ_HEAD(slot) & MPZ_SLOT_FLAG_HUGE)) {
			_mpz_slab_free(pool, slot);
		} else {
			_mpz_slab_push(pool, slab);
			_mpz_slab_init(pool, slab, MPZ_SLOT_READ_SIZE(slot));
		}

		slab = next;
	}
}

static inline mpz_void_t *_mpz_palloc(
	mpz_pool_t *pool, mpz_size_t size, mpz_cuint_t zeroize
) {
	mpz_slab_t *slab;
	mpz_slot_t *slot;
	mpz_uint_t  idx;

	MPZ_CHECK_NULL(pool);

	if (size < MPZ_POOL_MIN_ALLOC) {
		size = MPZ_POOL_MIN_ALLOC;
	} else if (size > MPZ_POOL_MAX_ALLOC) {
		return NULL;
	}

	size = mpz_align(size, MPZ_SLOTS_ALIGNMENT);

	if (size > (MPZ_BINS << MPZ_BINS_BIT_SHIFT)) {
		/* We have to grab a new memory space from the OS. */
		MPZ_CHECK_NULL(slab = _mpz_slab_create(pool, size + MPZ_SLOT_SIZE));

		/* The new slab contains only a single huge slot. */
		_mpz_slot_init(slot = MPZ_SLAB_TO_SLOT(slab), size, MPZ_SLOT_FLAG_HUGE|MPZ_SLOT_FLAG_USED);

		return MPZ_SLOT_TO_DATA(slot);
	}

	idx = MPZ_BIN_IDX(size);

	if (NULL == (slot = pool->bins[idx])) {
		/**
		 * The pool is either completely empty (new) or consists of slabs
		 * without empty slots for the requested size in the bins-list.
		 * We have to grab a new memory space from the OS.
		*/
		MPZ_CHECK_NULL(slab = _mpz_slab_create(pool, (size + MPZ_SLOT_SIZE) * MPZ_SLAB_ALLOC_MUL));

		_mpz_slab_init(pool, slab, size);
	}

	/* Pop a slot from the bins-array. */
	slot = pool->bins[idx];
	pool->bins[idx] = slot->next;

#ifdef MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS

	/* Mark the slot as "used". */
	_mpz_slot_init(slot, size, MPZ_SLOT_FLAG_USED);

#endif /* MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS */

	if (zeroize) {
		mpz_memzero(MPZ_SLOT_TO_DATA(slot), size);
	}

	return MPZ_SLOT_TO_DATA(slot);
}

static inline mpz_void_t *_mpz_slab_create(
	mpz_pool_t *pool, mpz_csize_t size
) {
	mpz_slab_t *slab;

	MPZ_CHECK_NULL(slab = mpz_memalign(MPZ_ALLOC_ALIGNMENT, size + MPZ_SLAB_SIZE));

	_mpz_slab_push(pool, slab);

	return slab;
}

static inline mpz_void_t _mpz_slab_init(
	mpz_pool_t *pool, mpz_slab_t *slab, mpz_cuint32_t size
) {
	mpz_slot_t *slot = MPZ_SLAB_TO_SLOT(slab);
	mpz_uint_t  idx, i;

	idx = MPZ_BIN_IDX(size);

	for (i = 0; i < MPZ_SLAB_ALLOC_MUL; i++) {
		/* Set slot metadata. */
		_mpz_slot_init(slot, size, 0);

		if (i == (MPZ_SLAB_ALLOC_MUL - 1)) {
			/**
			 * Handle the last slot:
			 * 
			 * Append the content of the current bin at the
			 * last slot of our slab and let the first slot
			 * of our slab be the leader of the current bin.
			*/

			slot->next = pool->bins[idx];

			pool->bins[idx] = MPZ_SLAB_TO_SLOT(slab);
		} else {
			/* Jump to the next slot. */
			slot->next = (mpz_slot_t *)((mpz_uchar_t *)slot + (size + MPZ_SLOT_SIZE));

			slot = slot->next;
		}
	}
}

static inline mpz_void_t _mpz_slab_push(
	mpz_pool_t *pool, mpz_slab_t *slab
) {
	slab->prev = NULL;
	slab->next = pool->slabs;

	if (NULL != slab->next) {
		slab->next->prev = slab;
	}

	pool->slabs = slab;
}

static inline mpz_void_t _mpz_slab_free(
	mpz_pool_t *pool, mpz_slot_t *slot
) {
	mpz_slab_t *slab = MPZ_SLOT_TO_SLAB(slot);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	/* Remove the slab from the slabs list. */

	if (NULL != slab->prev) {
		slab->prev->next = slab->next;
	} else {
		pool->slabs = slab->next;
	}

	if (NULL != slab->next) {
		slab->next->prev = slab->prev;
	}

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	free(slab);
}

static inline mpz_void_t _mpz_slot_init(
	mpz_slot_t *slot, mpz_cuint32_t size, mpz_cuint32_t flags
) {
	/**
	 * We have always to reset current state
	 * using "0" in this bitwise operation.
	*/
	*MPZ_SLOT_READ_HEAD(slot) = (0 | flags | size);

#ifdef MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS

	*MPZ_SLOT_GOTO_FOOT(slot, size) = MPZ_FOOTER_MARK;

#endif /* MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS */

}
