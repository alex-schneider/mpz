# MEMORY-POOL-Z (MPZ)

The MPZ is a simple implementation, of a very fast and effective memory pool. It's
designed for using in applications with a lot of memory space allocations of the
repeatedly sizes up to 1.024 bytes (default) per allocation, e.g. for structs in
tokenization applications.

## Features

* Written in pure `C`.
* Doesn't require any external libraries.
* Optimized for 32 and 64 bit systems on little-endian machines.
* Prevents internal memory space fragmentation (freed memory space is reusable).
* Implements memory space alignment to avoid wasting CPU cycles when accessing the
  data.
* Implements a very fast reset of the allocated memory space to permitt the memory
  space to be efficiently reused, which in most memory pool implementations is
  either unavailable or quite slow.
* Constant time for the memory space allocations from the internal memory unless
  the MPZ needs to grab a new memory space from the OS.
* Constant time for the `free` operations to the internal memory space.
* Implements simple security checks for `segmentation faults` and `double free`
  errors.
* Very easy to modify or extend.

## Limitations

* The maximum allocation size from MPZ is limited to `2^29 - 1` (536.870.911) bytes
  per allocation request.
* The code compiles and runs on Linux systems. Other platforms haven't been tested.

## About MPZ

The MPZ implements memory space alignment to avoid wasting CPU cycles when accessing
the data. The alignment is continuously implemented for the internal purposes as
well as for the return pointers to the user. The alignment size for memory space
allocations from the OS is borrowed from GNU libc and is defined as `2 * sizeof(size_t)`
which is 16 bytes on 64 bit systems. By the way, the memory pool of [nginx](http://nginx.org)
is one of the fastest pools today (as of year 2018) and it also uses this alignment,
too. To allow a finer granulated usage of the memory space inside the MPZ, the MPZ
implements binning of `slots`. The size of the memory space alignment for `slots`
inside the MPZ is 8 bytes (default).

### Pool

The `pool` is the main object of MPZ. It's the owner of:

* an array of linked lists for the free `slots` in the `pool` grouped by aligned
  size of the `slots` (binning).
* a doubly-linked list of the allocated `slabs` from the OS (stack).

The size of a MPZ `pool` is 1.040 bytes on 64 bit systems.

### Slabs

Every memory block allocated from the OS in MPZ is called `slab`. A `slab` consists
of a small metadata (16 bytes on 64 bit systems) and of an 1-to-n number of
`slots`, where `n` is defined as `MPZ_SLAB_ALLOC_MUL` constant (default `16`).

### Regular slots

Every chunk allocated from the `pool` in MPZ is called `slot`. A `slot` consists
of a small metadata (32 bits for a header and 32 bits for a footer) and of a `data`
part. The `data` part is the memory space that is returned to a user on an
allocation request. The regular `slots` have a size of up to 1.024 bytes (default)
and are managed by the bins-array of the `pool`.

### Huge slots

In cases that the used needs memory space that is larger as the default of 1.024
bytes, the MPZ will allocate an extra memory space from the OS and assign this space
to a single `slot` in an extra `slab`. The differences to the regular `slots` are:

* The huge `slabs` aren't managed by the bins-array of the `pool` and they are
  consequently not reusable.
* If a huge `slab` is freed, their memory space is immediately released back to
  the OS.

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

#### Illustration of "regular" `slots` inside the bins-array

The header and footer are always euqals. This fact allows to detect segmentation
faults. The two highest bits are used for flags. Other bits represent the size of
the `slot`.

```markdown
               slot from slab x                                      slot from slab y
                                                   /
||==============================================|| \ ||==============================================||
||  header   |   data part   |  footer   |      || / ||  header   |   data part   |  footer   |      ||
||           |               |           |      || \ ||           |               |           |      ||
|| (32 bits) | (min 8 bytes) | (32 bits) | next || / || (32 bits) | (min 8 bytes) | (32 bits) | next ||
||===========================================\==|| \ ||===========================================\==||
                                              \    / /                                             \
                                               \____/                                             NULL
```

## MPZ API

### mpz_pool_create()

Allocates an aligned memory space for the `pool`, initializes the allocated memory
space to zero and returns a pointer to the `pool` object. Returns `NULL` if failed.

```c
mpz_pool_t *mpz_pool_create(mpz_void_t);
```

### mpz_pool_reset()

Resets the allocated memory space to permitt him to be efficiently reused by the
`pool`. Note that the memory space of huge `slots` is immediately released back
to the OS.

```c
mpz_void_t mpz_pool_reset(mpz_pool_t *pool);
```

### mpz_pool_destroy()

Destroys the `pool`. The total allocated memory space inclusive the memory space
of the `pool` itself is released back to the OS.

```c
mpz_void_t mpz_pool_destroy(mpz_pool_t *pool);
```

### mpz_pmalloc()

Allocates `size` bytes of the memory space from the `pool`. If the requested size
is greater than the default of 1.024 bytes, a huge `slot` is directly allocated
from the OS like described above in the section `Huge slots`. If the `pool` hasn't
enouth free memory space to serve the requested `size` of bytes, a new `slab` is
allocated from the OS. Returns `NULL` if failed.

```c
mpz_void_t *mpz_pmalloc(mpz_pool_t *pool, mpz_csize_t size);
```

### mpz_pcalloc()

Allocates `size` bytes of the memory space from the `pool` like `mpz_pmalloc()`
function. Additionaly, all bits of the allocated memory space are initialized with
zero. Returns `NULL` if failed.

```note
Note that the `mpz_pcalloc()` function is always slower than the `mpz_pmalloc()`,
because the initializing of the bits in the memory space with zero requires an extra
iteration over the requested memory space.
```

```c
mpz_void_t *mpz_pcalloc(mpz_pool_t *pool, mpz_csize_t size);
```

### mpz_free()

Releases the allocated memory space back to the `pool`. The memory space of huge
`slots` are immediately released back to the OS.

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
#include "mpz_alloc.h"

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
