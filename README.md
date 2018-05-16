# MEMORY-POOL-Z (MPZ)

The MPZ is a simple implementation, of a very fast and effective memory pool. It's
designed for using in applications with a lot of allocations of "the same" sizes up
to 1.024 bytes (default) per allocation, e.g. for structs in tokenization applications.

## Features

* Written in pure `C`.
* Doesn't require any external libraries.
* Optimized for 32 and 64 bit systems on little-endian machines.
* Prevents internal memory fragmentation (freed `slots` are reusable).
* Implements memory alignments for faster access to the `pool` and its internal
  members.
* Implements a very fast reset of the memory in the total `pool`, which in most
  memory pool implementations is either unavailable or quite slow.
* Constant time for any allocation operations from the `pool` memory unless the
  MPZ needs to grab a new memory from the OS.
* Constant time for any `free` operations inside the `pool` memory.
* Implements simple checks for `segmentation faults` and `double free` errors.
* Very easy to modify or extend.

## Limitations

* The allowed maximum allocation size from the `pool` is `2^29 - 1` bytes.
* The code compiles and runs on Linux systems. Other platforms haven't been tested.

## About MPZ

The MPZ implements memory alignment for the `pool`, for all `slabs` and all `slots`
to allow the user a quick access to these elements. When the data is aligned in the
memory you don't waste CPU cycles in order to access the data. The alignment size
for memory allocations from the OS is borrowed from GNU libc and is defined as
`2 * sizeof(size_t)` which is 16 bytes on 64 bit systems. By the way, the memory
pool of [nginx](http://nginx.org) is one of the fastest pools today (as of year
2018) and it also uses this alignment, too. To allow a finer granulated usage of
the memory inside the `pool`, the MPZ implements binning of `slots`. The internal
alignment size for the `slots` from the pool is 8 bytes.

### Pool

The `pool` is the main object of MPZ. It's the owner of:

* an array of linked lists for all free `slots` in the `pool` grouped by aligned
  size of the `slots` (binning).
* a doubly-linked list of all allocated `slabs` from the OS (stack).

The size of a MPZ `pool` is 1.040 bytes on 64 bit systems.

### Slabs

Any memory block allocated from the OS in MPZ is called `slab`. A `slab` consists
of a small metadata (16 bytes on 64 bit systems) and of an 1-to-n number of
`slots`, where `n` is defined as `MPZ_SLAB_ALLOC_MUL` constant (default `16`).

### "Regular" slots

Any chunk allocated from the `pool` in MPZ is called `slot`. A `slot` consists
of a small metadata (32 bits for the header and 32 bits for the footer) and of
a `data` part. The "regular" slots managed by the bins-array of the `pool`.

### "Huge" slots

In cases that the used needs memory space which is larger as 1.024 bytes in the
`pool`, the MPZ will allocate extra memory from the OS and assign this space to
a single `slot` in an extra `slab`. The `slot` is marked as "huge" and "used".
If the user resets the `pool`, the memory of any `slabs` containing "huge" `slots`
are immediately freed (released back to the OS) and the extra `slab` is released
from the `pool`. The "regular" `slabs` are not released and can internally be
reused by the `pool`.

### Illustrations

#### Illustration of `slabs` inside the `pool`

```markdown
||==============================================================||
||           slab one           ||           slab two           ||
||  metadata   |      slots     ||  metadata   |      slots     ||
||             |                ||             |                ||
|| prev | next | .............. || prev | next | .............. ||
||===\======\===================/===/=======\===================||
 \    \      \                 /   /         \
  \  NULL     \_______________/   /         NULL
   \_____________________________/
```

#### Illustration of the bins and `slots` inside the `pool`

```markdown
bin index       |    0 |  ... |    8 |  ... |  127 |
                |======|======|======|======|======|
size in bytes   |    8 |  ... |  256 |  ... | 1024 |
                |======|======|======|======|======|
                    |             |
slots               |             |
                |======|      |======|
                |      |      |      |
                |======|      |======|
                                  |
                                  |
                              |======|
                              |      |
                              |======|
```

#### Illustration of a "regular" `slot`

The header and footer are always euqals. This fact allows to detect segmentation
faults. The two highest bits are used for flags. Other bits represent the size of
the `slot`.

```markdown
||============================================||
||  header   |      data part     |  footer   ||
||           |                    |           ||
|| (32 bits) | (always 2^n bytes) | (32 bits) ||
||           | (  min 8 bytes   ) |           ||
||============================================||
```

## MPZ API

Creates a new MPZ `pool`. Returns `NULL` if failed.

```c
mpz_pool_t *mpz_pool_create(mpz_void_t);
```

Resets the `pool`. The memory of any `slabs` containing "huge" `slots` are
immediately freed (released back to the OS) and the `slab` is released from the
`pool`. The memory of any `slabs` containing "regular" `slots` are not released
and can internally be reused by the `pool`.

```c
mpz_void_t mpz_pool_reset(mpz_pool_t *pool);
```

Destroys the `pool`. All of allocated memory inclusive the memory of the `pool`
itself is released back to the OS.

```c
mpz_void_t mpz_pool_destroy(mpz_pool_t *pool);
```

Allocates `size` bytes of memory from the `pool`. If the requested size is greater
than 1.024 bytes, a "huge" `slot` is allocated directly from the OS like described
above in the section `Huge (large) slots`). Returns `NULL` if failed.

```c
mpz_void_t *mpz_pmalloc(mpz_pool_t *pool, mpz_csize_t size);
```

Allocates `size` bytes of memory from the `pool` like the `mpz_pmalloc()` function.
The difference to `mpz_pmalloc()` is that the `mpz_pcalloc()` sets allocated memory
to zero. Returns `NULL` if failed.

```c
mpz_void_t *mpz_pcalloc(mpz_pool_t *pool, mpz_csize_t size);
```

Releases the allocated memory back to the `pool`. The memory of any "huge" `slots`
are immediately freed (released back to the OS).

```c
mpz_void_t mpz_free(mpz_pool_t *pool, mpz_void_t *data);
```

## Using MPZ

The user should use the MPZ API. The direct usage of functions outside the MPZ-API
can lead to unpredictable consequences if there is a lack of knowledge about the
internal implementation. To modify or extend the MPZ see the description `FOR
DEVELOPER WHO WANT TO ADAPT THE MPZ TO THE REQUIREMENTS OF THEIR APPLICATIONS` in
the `mpz_core.h`.

### Example

```c
#include "mpz_api.h"

int example_function()
{
    mpz_pool_t  *pool;
    void        *data;

    pool = mpz_pool_create();

    if (NULL == pool) {
        return 1;
    }

    data = mpz_pmalloc(pool, 123);

    if (NULL == data) {
        mpz_pool_destroy(pool);

        return 1;
    }

    /* Do here something with the "data"... */

    mpz_free(pool, data);

    /* Do other operations with the pool... */

    mpz_pool_destroy(pool);

    return 0;
}
```
