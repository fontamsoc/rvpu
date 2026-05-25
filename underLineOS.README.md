# `_OS` API Reference ([HTML Version](https://fontamsoc.github.io/underLineOS.docs/))

> **underLineOS** — a baremetal RTOS built into newlib libc implementing SMP-capable preemptive multi-threading, software timers, mutexes, semaphores, and FIFOs, all exposed through `<_os.h>`.

---

## Table of contents

- [Core utilities](#core-utilities)
  - [container\_of()](#container_of)
  - [Atomic operations](#atomic-operations)
  - [Doubly-linked lists](#doubly-linked-lists)
  - [Time utilities](#time-utilities)
- [Software timer](#software-timer)
- [Interrupt](#interrupt)
- [Mutex](#mutex)
- [FIFO](#fifo)
- [Semaphore](#semaphore)
- [Thread scheduling & sleep](#thread-scheduling--sleep)
- [CPU & trap helpers](#cpu--trap-helpers)

---

## Core utilities

### `container_of()`

Recover the containing structure from a pointer to one of its members.

```c
#define container_of(PTR, TYPE, MEMBER)
```

**Parameters**

| Name     | Description                      |
|----------|----------------------------------|
| `PTR`    | Pointer to the structure member  |
| `TYPE`   | Container structure type         |
| `MEMBER` | Name of the member within `TYPE` |

**Returns** — Pointer to the enclosing structure.

**Example**

```c
struct node { int value; };
struct object { int id; struct node n; };

struct node *x = ...;
struct object *obj = container_of(x, struct object, n);
```

---

### Atomic operations

Lock-free atomic operations on a target memory location. All macros return the previous value at `PTR`.

```c
#define _xchg(PTR, VAL)       // Atomically exchange a value.
#define _atomic_add(PTR, VAL) // Atomically add a value.
#define _atomic_inc(PTR)      // Atomically increment.
#define _atomic_dec(PTR)      // Atomically decrement.
#define _atomic_and(PTR, VAL) // Atomically apply bitwise AND.
#define _atomic_or(PTR, VAL)  // Atomically apply bitwise OR.
#define _atomic_xor(PTR, VAL) // Atomically apply bitwise XOR.
```

**Parameters**

| Name  | Description                          |
|-------|--------------------------------------|
| `PTR` | Target memory location               |
| `VAL` | Value used to compute the new result |

**Returns** — Previous value stored at `PTR`.

---

### Doubly-linked lists

Intrusive circular doubly-linked list. Embed `_dlist_t` into your own structs and use `container_of()` to recover the parent.

```c
typedef struct _dlist {
    struct _dlist *prev;
    struct _dlist *next;
} _dlist_t;

// Clear list links, setting both prev and next to NULL.
static inline void _dlist_clr(_dlist_t *l);

// Initialize a circular list node by self-referencing.
static inline void _dlist_init(_dlist_t *l);

// Insert a node between two consecutive nodes.
static inline void _dlist_add(_dlist_t *l, _dlist_t *prev, _dlist_t *next);

// Remove entries between two nodes.
static inline void _dlist_del(_dlist_t *prev, _dlist_t *next);
```

---

### Time utilities

System clock utilities for representing and converting durations to cycle counts.

```c
typedef uint64_t _date_t; // System clock / cycle timestamp type.
#define _DATE_MAX         // Maximum representable timestamp (infinite timeout).
#define _SECS(X)          // Convert seconds to clock cycles.
#define _MSECS(X)         // Convert milliseconds to clock cycles.
#define _USECS(X)         // Convert microseconds to clock cycles.
#define _NSECS(X)         // Convert nanoseconds to clock cycles.
```

---

## Software timer

> [!NOTE]
> A software timer can only be re-armed or dis-armed by the CPU that armed it. This simplifies implementation because locking mechanisms are not needed.

### `_timer_t`

```c
typedef struct _timer {
    _dlist_t l;                 // Timer list linkage.
    _date_t e;                  // Expiration date.
    uintptr_t cpu;              // Owning CPU.
    void (*f)(struct _timer *); // Expiration callback.
} _timer_t;
```

### `_timer_init()`

Initialize a timer object. Must be called before arming.

```c
#define _timer_init(T, F)
```

| Name | Description                                 |
|------|---------------------------------------------|
| `T`  | Pointer to `_timer_t` object                |
| `F`  | Expiration callback `void (*f)(_timer_t *)` |

### `_timer_arm()`

Arm a timer to fire at an absolute expiration timestamp. Must be called from the owning CPU when re-arming.

```c
void _timer_arm(_timer_t *t, _date_t e);
```

| Name | Description                               |
|------|-------------------------------------------|
| `t`  | Initialized `_timer_t` object             |
| `e`  | Absolute expiration timestamp (`_date_t`) |

### `_timer_disarm()`

Disarm a previously armed timer. Must be called from the same CPU that armed it.

```c
void _timer_disarm(_timer_t *t);
```

| Name | Description                       |
|------|-----------------------------------|
| `t`  | Armed `_timer_t` object to disarm |

---

## Interrupt

### `_irq_t`

```c
typedef struct _irq {
    _dlist_t l;               // List linkage.
    uintptr_t n;              // IRQ number.
    void (*f)(struct _irq *); // IRQ callback.
} _irq_t;
```

### `_irq_init()`

Initialize an IRQ descriptor before registration.

```c
#define _irq_init(I, N, F)
```

| Name | Description                        |
|------|------------------------------------|
| `I`  | Pointer to `_irq_t` object         |
| `N`  | IRQ number                         |
| `F`  | IRQ callback `void (*f)(_irq_t *)` |

### `_irq_register()` / `_irq_unregister()`

```c
void _irq_register(_irq_t *i);   // Register an interrupt handler.
void _irq_unregister(_irq_t *i); // Unregister an interrupt handler.
```

| Name | Description                     |
|------|---------------------------------|
| `i`  | Initialized `_irq_t` descriptor |

---

## Mutex

### `_mutex_t`

Recursive-capable mutex with a blocking wait-queue.

```c
typedef struct {
    uintptr_t lock;   // Internal lock.
    void *owner;      // Owning thread.
    uintptr_t acqcnt; // Recursive acquisition count.
    _waitq_t waitq;   // Wait-queue.
} _mutex_t;
```

### `_mutex_lock()`

Acquire a mutex, blocking until available or the timeout elapses.

```c
uintptr_t _mutex_lock(_mutex_t *m, _date_t timeout);
```

| Name      | Description                                              |
|-----------|----------------------------------------------------------|
| `m`       | Mutex object                                             |
| `timeout` | Max wait duration — use `_DATE_MAX` to wait indefinitely |

**Returns** — Non-zero on success, `0` on timeout.

### `_mutex_unlock()`

Release a previously acquired mutex, waking any waiting threads.

```c
void _mutex_unlock(_mutex_t *m);
```

| Name | Description             |
|------|-------------------------|
| `m`  | Mutex object to release |

### `_mutex_lock_recursive()` / `_mutex_unlock_recursive()`

Recursive variants — the same thread may acquire the mutex multiple times without deadlocking. Each lock must be paired with a corresponding unlock.

```c
uintptr_t _mutex_lock_recursive(_mutex_t *m, _date_t timeout);
void _mutex_unlock_recursive(_mutex_t *m);
```

---

## FIFO

### `_fifo_t`

```c
typedef struct {
    uintptr_t lock;  // FIFO lock.
    uintptr_t widx;  // Write index.
    uintptr_t ridx;  // Read index.
    void *buf;       // Buffer.
    size_t sz;       // Buffer size.
    _waitq_t wwaitq; // Writers wait-queue.
    _waitq_t rwaitq; // Readers wait-queue.
} _fifo_t;
```

### `_fifo_init()`

Initialize a FIFO with a caller-supplied backing buffer.

```c
void _fifo_init(_fifo_t *f, void *buf, size_t sz);
```

| Name  | Description                     |
|-------|---------------------------------|
| `f`   | FIFO object                     |
| `buf` | Caller-allocated backing buffer |
| `sz`  | Buffer size in bytes            |

### `_fifo_put()`

Write data into a FIFO, blocking if full until space is available or the timeout elapses.

```c
size_t _fifo_put(_fifo_t *f, void *buf, size_t sz, _date_t timeout);
```

| Name      | Description              |
|-----------|--------------------------|
| `f`       | FIFO object              |
| `buf`     | Source data buffer       |
| `sz`      | Number of bytes to write |
| `timeout` | Max wait if FIFO is full |

**Returns** — `sz` on success, `0` on timeout. Transfers are all-or-nothing — partial writes never occur.

### `_fifo_get()`

Read data from a FIFO, blocking if empty. Setting `peek` to `true` reads without consuming.

```c
size_t _fifo_get(_fifo_t *f, void *buf, size_t sz, bool peek, _date_t timeout);
```

| Name      | Description                                    |
|-----------|------------------------------------------------|
| `f`       | FIFO object                                    |
| `buf`     | Destination buffer                             |
| `sz`      | Number of bytes to read                        |
| `peek`    | `true` to leave data in the FIFO after reading |
| `timeout` | Max wait if FIFO is empty                      |

**Returns** — `sz` on success, `0` on timeout. Transfers are all-or-nothing — partial reads never occur.

### Utility macros

```c
#define _fifo_flush(X)          // Flush FIFO contents, emptying its buffer.
size_t _fifo_usage(_fifo_t *f); // Get current FIFO byte usage.
void _fifo_rst(_fifo_t *f);     // Reset FIFO empty, waking up any writers and readers.
```

---

## Semaphore

Semaphores are implemented as bounded FIFO counters. `_sem_t` is a type alias for `_fifo_t`.

```c
#define _sem_t _fifo_t

#define _SEM_DEF(X, N, I)  // Statically define and initialize a semaphore.
#define _sem_init(X, N, I) // Dynamically initialize a semaphore.
#define _sem_put(X, T)     // Release semaphore. Returns non-zero on success.
#define _sem_get(X, T)     // Acquire semaphore. Returns non-zero on success.
#define _sem_rst(X)        // Reset semaphore.
```

**Parameters**

| Name | Description                 |
|------|-----------------------------|
| `X`  | Semaphore object or name    |
| `N`  | Upper bound (maximum count) |
| `I`  | Initial count               |
| `T`  | Timeout (`_date_t`)         |

---

## Thread scheduling & sleep

### `_thread_t`

Thread object. Use [`_thread_create()`](#_thread_create) to obtain a `_thread_t *`.

```c
typedef struct {
    _dlist_t l;             // List linkage.
    enum {
        _THREAD_STOPPED = 0, // Thread is stopped. Covers both "not yet scheduled"
                             // and "sleeping on a wait-queue" (distinguished by wq).
        _THREAD_RUNNING = 1  // Thread is runnable or actively executing.
    } state;
    _waitq_t *wq;           // Wait-queue the thread is sleeping on, or null if not waiting.
    _timer_t z;             // Sleep timer.
    _date_t timeleft;       // Remaining runtime.
    void *stack;            // Stack base.
    uintptr_t cpu;          // Assigned CPU.
    bool pin;               // When true, the thread does not migrate.
    uintptr_t irq_disabled; // IRQ disable nesting depth; zero means IRQs are enabled.
    _savedctx_t *savedctx;  // Saved execution context.
} _thread_t;

_thread_t *_thread_cur; // Pointer to the currently running thread.
```

### `_thread_create()`

Create a new thread. The thread is not scheduled until `_thread_sched()` or `_thread_schedoncpu()` is called.

```c
_thread_t *_thread_create(
    void *stack,
    uintptr_t stacksz,
    void (*entry)(void *arg),
    void *arg);
```

| Name      | Description                              |
|-----------|------------------------------------------|
| `stack`   | Caller-allocated stack memory            |
| `stacksz` | Stack size in bytes                      |
| `entry`   | Thread entry point `void (*)(void *arg)` |
| `arg`     | Argument forwarded to the entry function |

**Returns** — Newly created `_thread_t *` object allocated from the top of the stack.

> [!NOTE]
> Passing `null` for `stack` causes the stack to be `malloc()`ed internally using `stacksz`. In that case `_thread_dispose()` will free it automatically.

### `_thread_sched()` / `_thread_schedoncpu()`

```c
// Schedule on any available CPU.
void _thread_sched(_thread_t *thrd);

// Schedule on a specific CPU.
void _thread_schedoncpu(_thread_t *thrd, uintptr_t cpu, bool pin);
```

| Name   | Description                               |
|--------|-------------------------------------------|
| `thrd` | Thread object to schedule                 |
| `cpu`  | Target CPU index                          |
| `pin`  | `true` to prevent load-balancer migration |

### `_thread_stop()` / `_thread_kill()` / `_thread_exit()` / `_thread_dispose()`

```c
void _thread_stop(_thread_t *thrd);    // Stop (pause) a thread.
void _thread_kill(_thread_t *thrd);    // Terminate a thread.
void _thread_exit(void);               // Terminate the current thread.
void _thread_dispose(_thread_t *thrd); // Release a terminated thread's resources.
```

### Sleep API

```c
#define _thread_sleep(D)                              // Sleep for a relative duration D.
#define _thread_sleepuntil(D)                         // Sleep until an absolute timestamp D.
#define _thread_sleeponwq(Q, D)                       // Sleep on wait-queue Q for duration D.
void _thread_sleeponwquntil(_waitq_t *wq, _date_t e); // Sleep on wait-queue until deadline.
```

### Preemption control & status checks

```c
void _preempt_disable(void);     // Disable thread preemption.
void _preempt_enable(void);      // Re-enable thread preemption.
#define _thread_yield()          // Voluntarily yield execution.

#define _is_thread_stopped(X)    // Non-zero if the thread is stopped.
#define _is_thread_terminated(X) // Non-zero if the thread has terminated.
#define _is_thread_running(X)    // Non-zero if the thread is runnable.
```

### `_waitq_t`

A queue of sleeping threads awaiting a condition. Used internally by mutex and FIFO, and available for custom synchronization primitives.

```c
typedef struct {
    uintptr_t lock; // Queue lock.
    uintptr_t p;    // Race-condition prevention state.
    void *l;        // Circular linked list of waiting threads.
} _waitq_t;

void _thread_schedone(_waitq_t *wq); // Wake one waiting thread.
void _thread_schedall(_waitq_t *wq); // Wake all waiting threads.
```

---

## CPU & trap helpers

### `_cpuid()` / `_clkfreq()` / `_clkcycles()` / `_ncpu()` / `_oops()`

```c
#define _cpuid()          // Get the current CPU ID.
#define _clkfreq()        // Get system clock frequency (Hz).
_date_t _clkcycles(void); // Read current clock cycle counter.
uintptr_t _ncpu(void);    // Returns the number of CPUs that have booted.
#define _oops()           // Fatal shutdown with diagnostic state dump.
```

### `_trap_savedctx()`

Returns the saved trap context pointer. The return value is non-null only while a trap is actively being handled — use this to inspect register state during fault handling or trap dispatch.

```c
#define _trap_savedctx()
```
