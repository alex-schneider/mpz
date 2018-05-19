
/**
 * Copyright (c) Alex Schneider
 */

#ifndef _MEMPOOLZ_CORE_H_INCLUDE
#define _MEMPOOLZ_CORE_H_INCLUDE

/* ==================================================================================================== */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ==================================================================================================== */
/* ==================================================================================================== */
/* ==================================================================================================== */

/* FOR DEVELOPER WHO WANT TO ADAPT THE MPZ TO THE REQUIREMENTS OF THEIR APPLICATIONS */

/**
 * Every project is unique and therefore has also individual requirements. Below
 * are some of the most important settings explained that developers can try out
 * for fine tuning of MPZ for their applications.
 * 
 * 
 * IMPORTANT INTRODUCTION:
 * 
 * Among other things, MPZ is so fast because it works without expensive arithmetic
 * operations (e.g. divisions or modulo-operator) and works with a lot of very
 * fast bitshift operations. For that an important prerequisite must be fulfilled:
 * Using of power-of-two numbers for alignment, binning and indexing.
*/

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/**
 * The MPZ implements simple checks for "segmentation faults" and "double free"
 * errors. In cases that an error is detected MPZ immediately raises an "SIGSEGV"
 * error. To disable this behavior just comment out the following definement.
 * 
 * Disabling of this feature increases a little bit the performance of the MPZ.
*/
#define MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS

/**
 * Total number of indexes in the bins-array (0 - 127). You can increase this number,
 * e.g. to be able to serve larger memory space directly from the pool, instead of
 * again and again allocating huge slabs from the OS.
*/
#define MPZ_BINS             (128)

/**
 * This alignment is used at all allocations of slots from the pool. At the same
 * time it is also the difference in bytes between each bin in the bins-array. So
 * a slot with the size of 8 bytes is stored in the bin with the index 0 and a slot
 * with the size of 1.024 bytes is stored in the bin with the index 127. If this
 * constant is modified, don't forget to modify the following const MPZ_BINS_BIT_SHIFT,
 * too.
*/
#define MPZ_SLOTS_ALIGNMENT  (8)

/**
 * This is the bit-shift using to calculate the index of the bins-array, as well,
 * for example, to determine if the MPZ needs to allocate a huge slab from the OS.
 * The constant MPZ_SLOTS_ALIGNMENT described above is very closely linked to this
 * constant, so changes should be applied to both constants simultaneously.
*/
#define MPZ_BINS_BIT_SHIFT   (3)

/**
 * If a new slab has to be allocated from the OS, the MPZ uses this multiplier to
 * allocate MPZ_SLAB_ALLOC_MUL numbers of the same requested slot size simultaneously
 * and to be able immediately to provide the remaining slots to the user if needed.
 * 
 * Long detailed research has shown that a multiplier of 16 is optimal for both
 * small and large numbers of allocations, without the overhead becoming too large.
*/
#define MPZ_SLAB_ALLOC_MUL   (16)

/**
 * Since the given size has to be already aligned before calling this operation,
 * this operation is equals to "size / 8 - 1", but we don't want to use expensive
 * division operations.
*/
#define MPZ_BIN_IDX(size)    (((size) >> MPZ_BINS_BIT_SHIFT) - 1)

/* ==================================================================================================== */
/* ==================================================================================================== */
/* ==================================================================================================== */

typedef void                mpz_void_t;
typedef const void          mpz_cvoid_t;
typedef unsigned char       mpz_uchar_t;
typedef unsigned int        mpz_uint_t;
typedef const unsigned int  mpz_cuint_t;
typedef uint32_t            mpz_uint32_t;
typedef const uint32_t      mpz_cuint32_t;
typedef size_t              mpz_size_t;
typedef const size_t        mpz_csize_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

typedef struct _mpz_pool_s  mpz_pool_t;
typedef struct _mpz_slab_s  mpz_slab_t;
typedef struct _mpz_slot_s  mpz_slot_t;

struct _mpz_slot_s
{
	mpz_void_t *data;
	mpz_slot_t *next;
};

struct _mpz_slab_s
{
	mpz_slab_t *prev;
	mpz_slab_t *next;
};

struct _mpz_pool_s
{
	mpz_slot_t *bins[MPZ_BINS];
	mpz_slab_t *slabs;
};

/* ==================================================================================================== */

#ifdef MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS
#include <signal.h>
#endif /* MPZ_RAISE_SIGSEGV_ON_MEM_ERRORS */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "mpz_alloc.h"

/* ==================================================================================================== */

#endif /* _MEMPOOLZ_CORE_H_INCLUDE */
