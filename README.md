[![Software license][ico-license]](https://github.com/alex-schneider/mpz/blob/master/LICENSE)
[![Latest Release][ico-release]](https://github.com/alex-schneider/mpz/releases/latest)

# MEMORY-POOL-Z (MPZ)

The MPZ is a simple implementation, of a very fast and effective memory pool. It's
designed for using in applications with a lot of memory space allocations of the
repeatedly sizes up to 1.024 bytes (default, configurable) per allocation, e.g.
for structs in tokenization applications.

## Features

* Written in pure `C`.
* Doesn't require any external libraries.
* Optimized for 32 and 64 bit systems on little-endian machines.
* Prevents internal memory space fragmentation (freed memory space is reusable).
* Implements [memory space alignment](https://en.wikipedia.org/wiki/Data_structure_alignment)
  to avoid wasting CPU cycles when accessing the data.
* Implements a very fast reset of the allocated memory space to permitt the memory
  space to be efficiently reused, which in most memory pool implementations is
  either unavailable or quite slow.
* Constant time for the memory space allocations from the internal memory space
  unless the MPZ needs to grab a new memory space from the OS via `malloc()`.
* Constant time for the `free` operations unless the MPZ needs to release the memory
  space back to the OS via `free()`.
* Implements simple security checks for `segmentation faults` and `double free`
  errors.
* Very easy to modify or extend.

## Limitations

* The maximum allocation size from MPZ is limited to `2^30 - 1` (1.073.741.823)
  bytes per allocation request.
* The code compiles and runs on Linux systems. Other platforms haven't been tested.

## TODOs

* Implementation of a `mpz_prealloc()` function.

## Bugs

* No bugs known!

## About MPZ

The MPZ implements memory space alignment to avoid wasting CPU cycles when accessing
the data. The alignment is continuously implemented for the accessing internal metadata
as well as for the return pointers to the user. The alignment size for memory space
allocations from the OS is borrowed from GNU libc and is defined as `2 * sizeof(size_t)`
which is 16 bytes on 64 bit systems. By the way, the memory pool of [nginx](http://nginx.org)
is one of the fastest pools today (as of year 2018) and it also uses this alignment,
too. To allow a finer granulated usage of the memory space inside the MPZ, the MPZ
implements binning of `slots`. The size of the memory space alignment for `slots`
inside the MPZ is 8 bytes (default, configurable).

### Pool

The `pool` is the main object of MPZ. It's the owner of:

* an array of linked lists for the free `slots` in the `pool` grouped by aligned
  size of the `slots` (binning).
* a doubly-linked list of the allocated `slabs` from the OS (stack).

The size of a MPZ `pool` is 1.040 bytes on 64 bit systems.

### Slabs

Every memory block allocated from the OS in MPZ is called `slab`. A `slab` consists
of a small metadata (16 bytes on 64 bit systems) and of an 1-to-n number of
`slots`, where `n` is defined as `MPZ_SLAB_ALLOC_MUL` constant (default `16`,
configurable).

### Regular slots

Every chunk allocated from the `pool` in MPZ is called `slot`. A `slot` consists
of a small metadata (32 bits for a header and 32 bits for a footer) and of a `data`
part. The `data` part is the memory space that is returned to a user on an
allocation request. The regular `slots` have a size of up to 1.024 bytes (default,
configurable) and are managed by the bins-array of the `pool`.

### Huge slots

In cases that the used needs memory space that is larger as the configurable default
of 1.024 bytes, the MPZ will allocate an extra memory space from the OS and assign
this space to a single `slot` in an extra `slab`. The differences to the regular
`slots` are:

* The huge `slabs` aren't managed by the bins-array of the `pool` and they are
  consequently not reusable.
* If a huge `slab` is freed, their memory space is immediately released back to
  the OS.

### Illustrations

#### Illustration of a MPZ `pool`

```markdown
SLABS (stack)

                                                 /
||============================================|| \ ||=====================||
||        slab 1       ||        slab 2       || / ||        slab n       ||
||                     ||                     || \ ||                     ||
||  metadata   | slots ||  metadata   | slots || / ||  metadata   | slots ||
||             |       ||             |       || \ ||             |       ||
|| prev | next | ..... || prev | next | ..... || / || prev | next | ..... ||
||===\======\==========/===/=======\==========|| \ ||==/=======\==========||
 \    \      \        /   /         \            /    /         \
  \    \      \      /   /           \          ...  /           \
   \  NULL     \____/   /             \________/   \/           NULL
    \__________________/


BINS (binning)

||===============================================================||
||     0 | ..... |     7 |     8 | ..... |    96 | ..... |   127 ||   <<< bin-index
||---------------------------------------------------------------||
||     8 | ..... |    64 |    72 | ..... |   776 | ..... |  1024 ||   <<< size in bytes per slot
||===============================================================||
     |               |                       |
     |               |                       |
 |=======|       |=======|               |=======|
 |       |       |       |               |       |
 |=======|       |=======|               |=======|
                     |                       |
                     |                       |                        <<< free slots
                 |=======|               |=======|
                 |       |               |       |
                 |=======|               |=======|
                     |
                     |
                 |=======|
                 |       |
                 |=======|

```

#### Illustration of the regular MPZ `slots` inside the bins-list

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

#### Illustration of a MPZ `slot` header

The header contains the metadata of the `slot`. All of the 32 bits of the footer
a filled with `1`. This fact allows to detect `segmentation fault` errors if an
application writes over the allocated memory space (unreliable but simple).

```markdown
=======
|  31 |   <<< "huge" flag (MSB on little-endian machines)
-------
|  30 |   <<< "used" flag (allows to detect "double free" errors)
-------
|  29 |   <<< currently unused (reserved for possible extensions)
-------
|  28 |\
------- \
| ... |   <<< represent the slot size
------- /
|   0 |/  (LSB on little-endian machines)
=======
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
is greater than the configurable default of 1.024 bytes, a huge `slot` is directly
allocated from the OS like described above in the section `Huge slots`. If the `pool`
hasn't enouth free memory space to serve the requested `size` of bytes, a new `slab`
is allocated from the OS. Returns `NULL` if failed.

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

The MPZ should be used wisely. Note that the MPZ is a high-level application. It's
a simple interface between the user and the `malloc()` and the `free()` functions.
The MPZ doesn't interact with the OS directly. Any invalid read/write errors are
difficultly to detect than using `malloc()` function directly. For example, many
users forget to allocate an extra byte to terminate a string with a `\0` byte. Here
is a big difference between the allocation of memory space from the `malloc()` function
and the MPZ. If the user allocates 5 bytes of memory space from the MPZ to write
a `Hello` string into the allocated memory space, the MPZ provides at least 8 bytes
of the memory space, where, by contrast, the `malloc()` function would provide only
5 bytes of really usable memory space. The invalid writing of the final 0-byte could
easy be detected by the direct usage of the `malloc()` function. Using the MPZ,
this error wouldn't be detected.

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

[ico-license]: https://img.shields.io/github/license/mashape/apistatus.svg
[ico-release]: https://img.shields.io/github/release/alex-schneider/mpz.svg
