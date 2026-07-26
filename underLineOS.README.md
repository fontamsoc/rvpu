# `_OS` API Reference ([HTML Version](https://fontamsoc.github.io/underLineOS.docs/))

> **underLineOS** — a baremetal RTOS built into newlib libc implementing SMP-capable preemptive multi-threading, software timers, mutexes, semaphores, and FIFOs, all exposed through `<_os.h>`.

---

## Table of contents

- [Timeout semantics](#timeout-semantics)
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
- [Scheduler internals](#scheduler-internals)
- [Boot & memory layout](#boot--memory-layout)
- [Usage rules & known limitations](#usage-rules--known-limitations)
- [CPU & trap helpers](#cpu--trap-helpers)
- [GDB stub (-lgdbstub)](#gdb-stub--lgdbstub)

---

## Timeout semantics

Every blocking primitive (`_mutex_lock()`, `_fifo_put()`, `_fifo_get()`, `_sem_put()`, `_sem_get()`) takes a `_date_t timeout` argument with the same meaning:

| Value       | Behavior                                                                                     |
|-------------|----------------------------------------------------------------------------------------------|
| `0`         | Non-blocking try. May also fail spuriously if the primitive's internal lock is momentarily contended by another CPU. |
| duration    | Maximum wait, in clock cycles — build it with `_SECS()`, `_MSECS()`, `_USECS()`, `_NSECS()`. |
| `_DATE_MAX` | Wait indefinitely; the call only returns on success.                                          |

A woken-up waiter that loses the race for the resource against another thread automatically sleeps again for the remainder of its timeout.

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

> [!IMPORTANT]
> Atomic instructions do **not** participate in the cache-coherency protocol. A variable manipulated with these macros must be accessed *exclusively* through them — a plain load of an atomically-written variable (or an atomic access to a plainly-written variable) can observe a stale value. Consequently, an atomically manipulated variable located in runtime-allocated memory must also be *initialized* atomically (e.g. `_xchg(&var, 0)`); statically allocated variables are safe, as their memory starts zeroed. `_fifo_init()`, `_sem_init()` and [`_mutex_init()`](#mutex) do this for the lock fields of their objects.

---

### Doubly-linked lists

Intrusive circular doubly-linked list. Embed `_dlist_t` into your own structs and use `container_of()` to recover the parent.

```c
typedef struct _dlist {
    struct _dlist *prev;
    struct _dlist *next;
} _dlist_t;

// Compound literal with both prev and next null; assign it
// to clear a node's links (an unlinked node has null links).
#define _DLIST_NIL (_dlist_t){0, 0}

// Initialize a circular list node by self-referencing.
static inline void _dlist_init(_dlist_t *l);

// Insert a node between two consecutive nodes.
static inline void _dlist_add(_dlist_t *l, _dlist_t *prev, _dlist_t *next);

// Remove entries between two nodes.
static inline void _dlist_del(_dlist_t *prev, _dlist_t *next);
```

> [!NOTE]
> `_dlist_del()` relinks the neighbors but does not clear the removed node's own `prev`/`next`; assign `_DLIST_NIL` to the node afterwards if its linkage state is later tested.

---

### Time utilities

System clock utilities for representing and converting durations to cycle counts. The conversion macros read `_clkfreq()` and compute in 64 bits, so they do not overflow for practical durations.

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

> [!IMPORTANT]
> **Callback context** — timer callbacks run inside the machine-mode trap handler of the owning CPU, with IRQs disabled. When the timer interrupted the idle loop, the thread-pointer register is null, so callbacks must not use TLS variables or newlib functions that rely on them (e.g. `printf()`), must not call `malloc()`, and must never call a blocking API (`_mutex_lock()`, `_fifo_put()`/`_fifo_get()`, `_thread_sleep*()`). A callback may re-arm its own timer and may wake threads with `_thread_schedone()`/`_thread_schedall()`/`_thread_sched()`.

### `_timer_t`

```c
typedef struct _timer {
    _dlist_t l;                 // Timer list linkage.
    _date_t e;                  // Expiration date.
    uintptr_t cpu;              // Owning CPU.
    void (*f)(struct _timer *); // Expiration callback.
} _timer_t;
```

### `_timer_init()` / `_TIMER_DEF()`

Initialize a timer object. Must be done before arming.

```c
#define _timer_init(T, F)  // Initialize at runtime.
#define _TIMER_DEF(X, F)   // Define and statically initialize a _timer_t named X.
```

| Name | Description                                 |
|------|---------------------------------------------|
| `T`  | Pointer to `_timer_t` object                |
| `X`  | Name of the `_timer_t` variable to define   |
| `F`  | Expiration callback `void (*f)(_timer_t *)` |

### `_timer_arm()`

Arm a timer to fire at an absolute expiration timestamp. Must be called from the owning CPU when re-arming. An expiration date already in the past makes the timer fire immediately.

```c
void _timer_arm(_timer_t *t, _date_t e);
```

| Name | Description                               |
|------|-------------------------------------------|
| `t`  | Initialized `_timer_t` object             |
| `e`  | Absolute expiration timestamp (`_date_t`) |

### `_timer_disarm()`

Disarm a previously armed timer. Must be called from the same CPU that armed it. Does nothing if the timer already expired or was never armed.

```c
void _timer_disarm(_timer_t *t);
```

| Name | Description                       |
|------|-----------------------------------|
| `t`  | Armed `_timer_t` object to disarm |

---

## Interrupt

All registered `_irq_t` descriptors live on a single list shared by all CPUs, so a device IRQ needs to be registered only once regardless of which CPU ends up handling it. Every registered descriptor whose number matches the acknowledged interrupt source gets its callback called.

> [!IMPORTANT]
> **Callback context** — IRQ callbacks run inside the machine-mode trap handler with IRQs disabled, under the same restrictions as timer callbacks: no TLS/newlib reliance when idle was interrupted, no `malloc()`, no blocking API calls. The IRQ number `-1` is reserved for inter-processor interrupts (IPIs) used by the scheduler.

Back-to-back pending interrupts are dispatched within a single trap entry on the live saved context ("tail-chaining", see [Scheduler internals](#scheduler-internals)): consecutive timer and IRQ callbacks may run without the interrupted context being restored and re-saved in between. This is transparent to callback authors — the callback context restrictions above already cover it.

### `_irq_t`

```c
typedef struct _irq {
    _dlist_t l;               // List linkage.
    uintptr_t n;              // IRQ number.
    void (*f)(struct _irq *); // IRQ callback.
} _irq_t;
```

### `_irq_init()` / `_IRQ_DEF()`

Initialize an IRQ descriptor before registration.

```c
#define _irq_init(I, N, F)  // Initialize at runtime.
#define _IRQ_DEF(X, N, F)   // Define and statically initialize an _irq_t named X.
```

| Name | Description                        |
|------|------------------------------------|
| `I`  | Pointer to `_irq_t` object         |
| `X`  | Name of the `_irq_t` variable to define |
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

Recursive-capable mutex with a blocking wait-queue. Statically initialize with `_MUTEX_NIL`; a `_mutex_t` in runtime-allocated memory must be initialized with `_mutex_init()`, which atomically initializes its lock fields (see the [atomic operations](#atomic-operations) coherency note).

```c
typedef struct {
    uintptr_t lock;   // Internal lock.
    void *owner;      // Owning thread.
    uintptr_t acqcnt; // Recursive acquisition count.
    _waitq_t waitq;   // Wait-queue.
} _mutex_t;

#define _MUTEX_NIL (_mutex_t){0, 0, 0, _WAITQ_NIL} // Static initializer.
#define _mutex_init(X) // Initializer for a _mutex_t in runtime-allocated memory.
```

### `_mutex_lock()`

Acquire a mutex, blocking until available or the timeout elapses. If a woken-up waiter loses the acquisition race against another thread, it sleeps again for the remainder of its timeout; with `_DATE_MAX` it never fails.

```c
uintptr_t _mutex_lock(_mutex_t *m, _date_t timeout);
```

| Name      | Description                                              |
|-----------|----------------------------------------------------------|
| `m`       | Mutex object                                             |
| `timeout` | See [Timeout semantics](#timeout-semantics)              |

**Returns** — Non-zero on success, `0` on timeout.

> [!WARNING]
> The non-recursive mutex is not reentrant: a thread re-locking a mutex it already owns blocks on itself (deadlock, or timeout failure). Use the recursive variants for reentrancy.

### `_mutex_unlock()`

Release a previously acquired mutex, waking one waiting thread.

```c
void _mutex_unlock(_mutex_t *m);
```

| Name | Description             |
|------|-------------------------|
| `m`  | Mutex object to release |

> [!WARNING]
> No ownership check is performed: unlocking a mutex the calling thread does not own releases it regardless. Locking discipline is the caller's responsibility (the recursive variants do check ownership).

### `_mutex_lock_recursive()` / `_mutex_unlock_recursive()`

Recursive variants — the same thread may acquire the mutex multiple times without deadlocking. Each lock must be paired with a corresponding unlock. `_mutex_unlock_recursive()` on a mutex not owned by the calling thread is a fatal error (`_oops()`).

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
#define _fifo_init(F, B, S)
```

| Name  | Description                     |
|-------|---------------------------------|
| `F`   | FIFO object                     |
| `B`   | Caller-allocated backing buffer, or `NULL` for a counting-only FIFO |
| `S`   | Buffer size in bytes            |

> [!NOTE]
> With a `NULL` buffer the FIFO becomes a pure counter of capacity `S`: `_fifo_put()`/`_fifo_get()` move the indexes without copying data. This is what [semaphores](#semaphore) are built on.

### `_fifo_put()`

Write data into a FIFO, blocking if full until space is available or the timeout elapses.

```c
size_t _fifo_put(_fifo_t *f, void *buf, size_t sz, _date_t timeout);
```

| Name      | Description              |
|-----------|--------------------------|
| `f`       | FIFO object              |
| `buf`     | Source data buffer; ignored if `NULL`, or if the FIFO was initialized with a `NULL` buffer |
| `sz`      | Number of bytes to write |
| `timeout` | See [Timeout semantics](#timeout-semantics) |

**Returns** — `sz` on success, `0` on timeout. Transfers are all-or-nothing — partial writes never occur.

### `_fifo_get()`

Read data from a FIFO, blocking if there is less data than requested. Setting `peek` to `true` reads without consuming. Passing `-1` as `sz` does not block, and instead drains whatever is currently in the FIFO, returning the amount removed.

```c
size_t _fifo_get(_fifo_t *f, void *buf, size_t sz, bool peek, _date_t timeout);
```

| Name      | Description                                    |
|-----------|------------------------------------------------|
| `f`       | FIFO object                                    |
| `buf`     | Destination buffer; ignored if `NULL`, or if the FIFO was initialized with a `NULL` buffer |
| `sz`      | Number of bytes to read, or `-1` to drain      |
| `peek`    | `true` to leave data in the FIFO after reading |
| `timeout` | See [Timeout semantics](#timeout-semantics)    |

**Returns** — `sz` on success (the amount drained when `sz` is `-1`), `0` on timeout. Transfers are all-or-nothing — partial reads never occur.

### Utility macros

```c
#define _fifo_flush(X)          // Flush FIFO contents, emptying its buffer.
size_t _fifo_usage(_fifo_t *f); // Get current FIFO byte usage.
void _fifo_rst(_fifo_t *f);     // Reset FIFO empty, waking up any writers and readers.
```

---

## Semaphore

Semaphores are implemented as bounded FIFO counters (FIFOs initialized with a `NULL` buffer). `_sem_t` is a type alias for `_fifo_t`.

```c
#define _sem_t _fifo_t

#define _SEM_DEF(X, N, I)  // Statically define and initialize a semaphore.
#define _sem_init(X, N, I) // Dynamically initialize a semaphore.
#define _sem_put(X, T)     // Release semaphore; blocks while the count is at its upper bound.
#define _sem_get(X, T)     // Acquire semaphore; blocks while the count is null.
#define _sem_rst(X)        // Reset semaphore.
```

`_sem_put()` and `_sem_get()` return non-zero on success, `0` on timeout.

**Parameters**

| Name | Description                 |
|------|-----------------------------|
| `X`  | Semaphore object or name    |
| `N`  | Upper bound (maximum count) |
| `I`  | Initial count               |
| `T`  | Timeout (`_date_t`) — see [Timeout semantics](#timeout-semantics) |

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
    uintptr_t claim;        // Serializes concurrent wake-up paths.
    _timer_t z;             // Sleep timer.
    _date_t timeleft;       // Remaining timeslice of a preempted thread.
    void *stack;            // Stack base (null unless internally malloc()ed).
    uintptr_t cpu;          // Assigned CPU.
    bool pin;               // When true, the thread does not migrate.
    uintptr_t irq_disabled; // IRQ disable nesting depth; zero means IRQs are enabled.
    _savedctx_t *savedctx;  // Saved execution context.
} _thread_t;

_thread_t *_thread_cur; // Pointer to the currently running thread (the tp register).
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

> [!IMPORTANT]
> The top of the stack is used for the thread's TLS image, its `_thread_t`, and its initial saved context; `stacksz` must therefore cover the program's TLS size (`__tbss_end - __tdata_start`) plus `sizeof(_thread_t)` plus `sizeof(_savedctx_t)`, in addition to the deepest call-stack the thread will need. Returning from `entry` terminates the thread as if it had called `_thread_exit()`.

### `_thread_sched()` / `_thread_schedoncpu()`

```c
// Schedule on the least-loaded CPU (unless pinned);
// does nothing if the thread is already running.
void _thread_sched(_thread_t *thrd);

// Schedule on a specific CPU; does nothing if the thread is already running.
void _thread_schedoncpu(_thread_t *thrd, uintptr_t cpu, bool pin);
```

| Name   | Description                               |
|--------|-------------------------------------------|
| `thrd` | Thread object to schedule                 |
| `cpu`  | Target CPU index                          |
| `pin`  | `true` to prevent load-balancer migration |

If the thread is on a `_waitq_t`, it gets removed from it. Neither call preempts `_thread_cur`; the scheduled thread starts running at the next scheduling point of the target CPU (which is immediate when the target CPU is idle or another CPU). Both calls do nothing on a thread that is already running: its wake-up is then already accomplished, and it is left where it is. To migrate a running thread, `_thread_stop()` it first, then reschedule it with `_thread_schedoncpu()`.

> [!NOTE]
> A thread woken up before its sleep timeout expired (its sleep timer is still armed) first resumes on the CPU that armed the timer — overriding `cpu` — so that the timer is disarmed on its owning CPU; the thread can migrate normally afterwards.

### `_thread_stop()` / `_thread_kill()` / `_thread_exit()` / `_thread_dispose()`

```c
void _thread_stop(_thread_t *thrd);    // Force-stop a thread; resumable with _thread_sched().
void _thread_kill(_thread_t *thrd);    // Terminate a thread; it can no longer be resumed.
void _thread_exit(void);               // Terminate the current thread.
void _thread_dispose(_thread_t *thrd); // Release a terminated thread's resources.
```

`_thread_stop()` and `_thread_kill()` do not preempt `_thread_cur`; stopping or killing the current thread takes effect at its next yield (`_thread_exit()` does both). `_thread_dispose()` must only be used once `_is_thread_terminated()` is true. See [Usage rules & known limitations](#usage-rules--known-limitations) for restrictions.

### Sleep API

```c
#define _thread_sleep(D)                              // Sleep for a relative duration D.
#define _thread_sleepuntil(D)                         // Sleep until an absolute timestamp D.
#define _thread_sleeponwq(Q, D)                       // Sleep on wait-queue Q for duration D.
void _thread_sleeponwquntil(_waitq_t *wq, _date_t e); // Sleep on wait-queue until deadline.
```

Passing `_DATE_MAX` as the duration/deadline sleeps indefinitely until the thread is woken up through `_thread_sched()`, `_thread_schedoncpu()`, `_thread_schedone()` or `_thread_schedall()`. Sleeping is allowed while preemption is disabled; the thread resumes with preemption still disabled.

### Preemption control & status checks

```c
void _preempt_disable(void);     // Disable preemption of the current thread.
void _preempt_enable(void);      // Re-enable preemption of the current thread.
#define _thread_yield()          // Voluntarily yield execution.
void _thread_preempt(uintptr_t cpu); // Preempt the thread currently running on a CPU.

#define _is_thread_stopped(X)    // Non-zero if the thread is stopped.
#define _is_thread_terminated(X) // Non-zero if the thread has terminated.
#define _is_thread_running(X)    // Non-zero if the thread is runnable.
```

`_preempt_disable()` disables all IRQs on the current CPU (not just the scheduler tick) and nests: preemption is re-enabled when `_preempt_enable()` has been called as many times as `_preempt_disable()`. Both are no-ops inside timer/IRQ callbacks, where IRQs are already disabled. `_preempt_enable()` without a matching `_preempt_disable()` is a fatal error (`_oops()`).

### `_waitq_t`

A queue of sleeping threads awaiting a condition. Used internally by mutex and FIFO, and available for custom synchronization primitives.

```c
typedef struct {
    uintptr_t lock; // Queue lock.
    uintptr_t p;    // Count of threads about to enqueue themselves (see below).
    void *l;        // Circular linked list of waiting threads.
} _waitq_t;

#define _WAITQ_NIL (_waitq_t){0, 0, 0}

void _thread_schedone(_waitq_t *wq); // Wake the thread that was first added to the queue.
void _thread_schedall(_waitq_t *wq); // Wake all threads in the queue, oldest first.
```

> [!NOTE]
> **The `p` protocol for custom primitives** — a thread that decides to sleep while holding its primitive's internal lock inevitably enqueues itself onto the wait-queue *after* releasing that lock; a waker running in that window would find the wait-queue empty and the wake-up would be lost. To close the window, increment `wq->p` with `_atomic_inc()` before releasing the primitive's lock; `_thread_sleeponwquntil()` decrements it once the thread is on the queue, and `_thread_schedone()`/`_thread_schedall()` wait for in-window threads to enqueue whenever the queue is empty while `wq->p` is non-null.

---

## Scheduler internals

- **Per-CPU runqueues** — each CPU owns a circular runqueue of `_THREAD_RUNNING` threads and schedules round-robin through it.
- **Timeslicing** — when a runqueue holds more than one thread, a per-CPU scheduling timer preempts the current thread after `SCHEDLRHZ / nthreads` cycles (`SCHEDLRHZ` defaults to 50 ms worth of cycles), which bounds the time a preempted thread waits to resume by `SCHEDLRHZ`. A thread preempted early (e.g. by a wake-up) keeps its remaining timeslice in `timeleft` and gets it back the next time it runs. With a single runnable thread, the scheduling timer is disarmed — a purely event-driven regime with zero scheduling overhead.
- **Load balancing** — `_thread_sched()` places the thread on the runqueue with the fewest threads, unless the thread is pinned (`pin`) or the system has a single CPU.
- **IPIs** — scheduling operations targeting another CPU kick it with an inter-processor interrupt through the interrupt controller (reserved IRQ number `-1`).
- **Idle** — a CPU with an empty runqueue parks in `wfi` inside the trap handler with a null thread pointer, waking on the next timer or external interrupt.
- **Trap tail-chaining** — before restoring the interrupted context, the trap-return path checks `mip & mie` and dispatches a pending machine-external or machine-timer interrupt (in that hardware priority order) directly on the live trap frame, skipping the restore + `mret` + hardware re-trap + re-save round-trip. Chaining is gated on the interrupted context having had IRQs enabled (saved `mstatus.MPIE`) — a context that had them disabled resumes untouched, exactly as the hardware would behave. Because the interrupt controller does not dispatch device interrupts to a CPU whose `mstatus.MIE` is clear, mid-handler external chaining effectively covers IPIs (which bypass that gate) and interrupts already pending at trap entry; a device interrupt raised mid-handler is delivered to another ready CPU or after the final `mret`, as before. The feature is compiled in when `USETAILCHAIN` is set to 1 in `_os_params.h`.
- **Timed sleeps** — `_thread_sleeponwquntil()` arms the thread's sleep timer on the current CPU. If the timeout expires, the timer's callback reschedules the thread on that same CPU. If the thread is woken up early instead, it first resumes on that CPU so the timer can be disarmed there (timers are strictly per-CPU), then may migrate normally.

---

## Boot & memory layout

**Boot flow (`crt0.S`)** — CPU 0 initializes `gp`, the trap vector, and the trap-disabled state, then: saves a pristine copy of `.data` below the top of RAM (or restores `.data` from it on a warm boot, keyed by `__rst_flg` — memory contents persist across reset), zeroes `.bss`, carves the main thread's TLS image and `_thread_t` from the top of the stack, records `__heap_ptr = _end` and `__heap_end = sp`, and calls `__init_multithreading()` — which registers the IPI handler, releases the secondary CPUs one at a time (each increments `__ncpu`), and initializes the runqueues — before running the usual libc init and `main()`. Harts with `mhartid >= NCPU` (the compile-time CPU maximum) park immediately.

**Secondary CPUs** wait for `__ncpu == mhartid`, briefly use a temporary trap stack below `__heap_end`, then switch to their per-CPU trap stack in `__trap_stacks[NCPU][TRAP_STACK_SIZE]` and park idle in `wfi` until scheduling work arrives.

**Memory layout** (addresses growing upward):

```
+--------------------------- top of RAM
| .data pristine copy       |  (for warm-boot restore)
+----------------------------
| main thread TLS image     |
| main thread _thread_t     |
+---------------------------- <- __heap_end (main thread sp at boot)
| main thread stack (grows down)
|            ...            |
| heap (grows up)           |
+---------------------------- <- _end / __heap_ptr at boot
| .bss / .data / .text      |
+--------------------------- start of RAM
```

> [!WARNING]
> The heap (`_sbrk()`) grows up toward `__heap_end` while the main thread's stack grows down from it — **there is no guard between them**; a deep main-thread stack and a large heap can silently collide. Threads created with `_thread_create()` use their own stacks and are not affected.

**TLS** — the `_thread_t` of every thread doubles as the first thread-local object: the linker script places the reserved `.tdata.__thread_cur` section at TLS offset 0, *before* the `__tdata_start` marker, so the runtime TLS copies exclude it while compiled TLS accesses (offsets from the `tp` register, which points to the `_thread_t`) line up. Each thread's TLS image and `_thread_t` are carved from the top of its stack by `_thread_create()`/`crt0`.

---

## Usage rules & known limitations

**Rules**

- Timer and IRQ callbacks must follow the [callback context restrictions](#software-timer): no blocking calls, no `malloc()`, no TLS/newlib reliance.
- Lifecycle operations on the *same* thread (`_thread_stop()`, `_thread_kill()`, `_thread_sched()`, `_thread_schedoncpu()`) must be serialized by the caller; only the wake-up paths (`_thread_schedone()`/`_thread_schedall()`/timeout expiry) are internally serialized against each other.
- Do not `_thread_stop()` or `_thread_kill()` a thread that is blocked inside a synchronization primitive (mutex/FIFO/semaphore); stop it at a point where it is running or sleeping via the plain sleep API. A killed in-window waiter would leak its wait-queue `p` token, and a force-stopped timed sleeper still has its sleep timer pending.
- `_thread_dispose()` only after `_is_thread_terminated()` is true; on SMP, be aware that a thread reports terminated slightly before its CPU finishes switching away from it — delay disposal (e.g. by one scheduler tick) when the freed stack could be reallocated immediately.

**Known limitations**

- `_thread_stop()` acting on a thread currently running on another CPU spins (with IRQs disabled) until that CPU preempts it; two CPUs concurrently doing this to each other's current threads deadlock. Avoid symmetric cross-CPU stop patterns.
- A terminated thread reports `_is_thread_terminated()` slightly before its CPU finishes switching away from it; delay `_thread_dispose()` accordingly when the freed stack could be reallocated immediately (context *restore* is fully handshaked via `_thread_t.ctxsaved`, but disposal is not).
- The TLS setup assumes `.tbss` immediately follows `.tdata` with no alignment gap, and honors TLS alignment only up to the pointer size; avoid over-aligned (`> XLEN`) thread-local variables.
- The heap/main-stack boundary is unguarded (see [Boot & memory layout](#boot--memory-layout)).

---

## CPU & trap helpers

### `_cpuid()` / `_clkfreq()` / `_clkcycles()` / `_ncpu()` / `_oops()`

```c
#define _cpuid()          // Get the current CPU ID (mhartid).
#define _clkfreq()        // Get system clock frequency in Hz (custom CSR 0xcc0).
_date_t _clkcycles(void); // Read current 64-bit clock cycle counter.
uintptr_t _ncpu(void);    // Returns the number of CPUs that have booted.
#define _oops()           // Fatal error: prints the CPU and PC, then halts the system
                          // (breaks into the debugger instead when the GDB stub is linked).
```

### `_trap_savedctx()`

Returns the saved trap context pointer (`_savedctx_t *`). The return value is non-null only while a trap is actively being handled — use this to inspect register state during fault handling or trap dispatch, or to detect trap context (timer/IRQ callbacks). During tail-chained dispatches (see [Scheduler internals](#scheduler-internals)) the `cause` field is updated for each dispatched interrupt, while `epc`/`tval` continue to describe the original trap (`epc` already advanced past a completed `ecall`).

```c
#define _trap_savedctx()
```

---

## GDB stub (-lgdbstub)

Linking `-lgdbstub` embeds a GDB Remote-Serial-Protocol stub (`_os_gdbstub.c`, built into `libgdbstub.a`) in the application; without it nothing changes — the exception handler stubs in `_os.c` (`__trap_exc_break` and friends) are `weak` precisely so that the archive can override them, and they are referenced by `crt0`'s exception dispatch table, which is what pulls the stub object out of the archive.

**What it provides** (all against the *running* application — attach at any time):

- Per-thread debugging: the stub runs in its own engine thread, the application keeps running while gdb sits at its prompt, and threads are individually stopped, inspected, stepped and resumed (see below).
- Software breakpoints (gdb plants `ebreak` itself through memory writes).
- Single-step, implemented stub-side (gdb itself provides no software single-step for bare-metal RISC-V targets): temporary `ebreak` are planted at the successor instructions; an `lr.w`/`sc.w` sequence is stepped over as a whole so the reservation is never destroyed; another thread hitting a step plant is held silently until the step ends.
- `Ctrl-C` stops the selected thread (SIGINT); everything else keeps running.
- Memory and register read/write, `detach`/re-attach (`detach` resumes every thread the stub stopped).
- Every exception becomes a debugger stop of the faulting thread instead of a `while(1)` hang: illegal instruction → SIGILL, misaligned load/store/jump → SIGBUS, access/page faults → SIGSEGV (where the hardware raises them), unexpected `ecall` → SIGTRAP.
- `_oops()` prints its banner then drops into the frozen emergency session (SIGTRAP) instead of halting the system.
- An `ebreak` executed before gdb ever attached parks that thread awaiting the debugger.

**Per-thread debugging** — the wire protocol stays the plain single-threaded RSP; the whole thread user-interface is one stub global plus two stub functions driven from gdb:

- `_thread_t *__gdbstub_tp` — the SELECTION: the thread whose registers gdb reads and writes, and which `continue`/`stepi`/`Ctrl-C` act on. Switch it at the prompt:

      (gdb) set var __gdbstub_tp = thrd_spin

  (any expression yielding a `_thread_t *`; exporting the application's thread pointers as globals makes this symbolic). At attach the selection is the stub's debug-shell thread: a resting stub context which perturbs nothing when inspected and hosts the inferior calls below.
- `Ctrl-C` parks the selection — and only it. A parked sleeping thread has its wake-up timer disarmed (its sleep truncates to the park duration).
- A thread hitting a breakpoint or faulting parks itself silently unless it is the selection; the queued stops are drained at the prompt with:

      (gdb) set var __gdbstub_tp = __gdbstub_next()

  which resumes the previously selected thread and adopts the oldest stopped one (the debug-shell when none is queued); `set var __gdbstub_tp = __gdbstub_last()` selects back the thread most recently resumed (unlike the drain, it resumes nothing: the thread being switched away from stays as it was); `print __gdbstub_parked` lists the stub-parked threads. Suggested convenience defines:

      define gn
        set var __gdbstub_tp = __gdbstub_next()
      end
      define gl
        set var __gdbstub_tp = __gdbstub_last()
      end

- Registers of a running thread read as `<unavailable>` — stop a thread before inspecting it. A cooperatively descheduled thread (blocked on a mutex, sleeping) shows its callee-saved registers (`ra`/`sp`/`s0`-`s11`; the caller-saved ones read `<unavailable>`) and backtraces correctly.
- Breakpoints stop whichever thread hits them; there is no per-thread breakpoint filtering (gdb's `break ... thread N` has no meaning here, gdb seeing a single thread) — the selection's hits report, everybody else's queue silently. Scope a breakpoint to a thread by placing it in code only that thread runs, or drain the unwanted stops.
- Recommended: `set breakpoint always-inserted on` — all-stop gdb otherwise removes its breakpoints from memory the whole time it sits at the prompt, and the running threads would sail through them.

Common prompt recipes:

- Stop a specific thread: `set var __gdbstub_tp = thrd_x`, `continue`, then `Ctrl-C`.
- Resume the selection and get the prompt back (rather than awaiting its next stop): drain with `set var __gdbstub_tp = __gdbstub_next()` — the selection resumes and the oldest queued stop (or the debug-shell) gets selected. This is also how a manually stopped thread is resumed: select it, then drain.
- Resume everything the stub stopped without detaching: for each entry of `print __gdbstub_parked` (the debug-shell excepted), select it then drain; `detach` + re-attach does it in one step.
- Park a thread for the debugger before gdb ever attached: execute `__asm__ __volatile__ ("ebreak")` in it; once attached, drain to adopt it, and — the `ebreak` being real code, not a gdb breakpoint — skip it before resuming: `set var $pc = $pc + 4`.

**Emergency session** — contexts which cannot be parked drop into a synchronous session servicing gdb from within the trap with the whole machine frozen; `continue` unfreezes. This covers: nested traps (ie: `_oops()`, or a breakpoint inside a trap/interrupt handler or timer callback), the idle context, the stub engine itself (ie: a breakpoint planted in stub internals), faults before the stub constructor ran, a full parked list, and any context interrupted with IRQs disabled (ie: a breakpoint inside a spinlock/`_preempt_disable` critical section, scheduler internals included — they are steppable, just with the world frozen). An `_oops()` reached while gdb sits at its prompt sends nothing unsolicited: gdb keeps displaying its cached registers; run `maintenance flush register-cache` (then `bt`) to see the frozen context — memory reads are live either way.

**Transport** — by default the serial channel at `0xe80` (irqctrl source 1, `serial_pty0` in rv32-sim). Four configuration symbols retarget it; the stub reads each symbol's *address* as the value (not the contents of a variable), so they are set from the link line with `--defsym`. Any subset may be given — an unset symbol keeps its default:

```sh
LDFLAGS += -Wl,--defsym=_gdbstub_dev=0xf88     # Device base (command register at +4). Default 0xe80.
LDFLAGS += -Wl,--defsym=_gdbstub_irq=1         # irqctrl source index. Default 1.
LDFLAGS += -Wl,--defsym=_gdbstub_membeg=0x1000 # gdb memory-access low bound. Default 0x1000 (RAM start).
LDFLAGS += -Wl,--defsym=_gdbstub_memend=0      # gdb memory-access high bound. Default 0 selects
                                               # __heap_end rounded up to 1KB (just below the RAM top;
                                               # the boot carve-out above __heap_end may be partially
                                               # excluded).
```

Do **not** retarget these by defining a strong C variable (`void *_gdbstub_dev = (void *)0xf88;`): the stub reads the symbol's *address*, so a strong variable makes it read that variable's storage location rather than `0xf88`, which hangs the bus. `--defsym` (or an equivalent absolute `.set` in a *separate* object) is the only working way — a default written inside the stub's own translation unit would be baked into the code as an immediate by the optimizer and could no longer be overridden. `0` is the "unset" sentinel, so `_gdbstub_irq` cannot select source 0 (the console source, never a gdb transport anyway).

Out-of-bounds gdb memory requests get error replies — an unmapped bus access would terminate rv32-sim.

**Usage with rv32-sim** — the pty must exist before the sim starts, and gdb opens the *other* end of the socat pair:

```sh
socat pty,link=/tmp/ttyGDB0,raw,echo=0 pty,link=/tmp/ttyGDB1,raw,echo=0 &
make clean sim run SERIAL_PTY0=/tmp/ttyGDB0 SERIAL_PTY_POLLCYCLES=64 \
    SRAM_INITFILE=apps/gdbstubtest/gdbstubtest.32.hex
riscv32-unknown-elf-gdb -nx -ex 'set arch riscv:rv32' -ex 'set remotetimeout unlimited' \
    -ex 'file apps/gdbstubtest/gdbstubtest.elf' -ex 'target remote /tmp/ttyGDB1'
```

`SERIAL_PTY_POLLCYCLES` (default 1024) throttles how often the sim polls the pty; 64 makes gdb I/O snappier. The `rv32-sim/apps/gdbstubtest` application exercises the whole matrix (breakpoints, per-thread stepping over `lr/sc`, its exported thread zoo, faults via its `fault_sel` variable, `_oops()`, dispose/respawn via `exit_req`).

**Transport pacing** — the engine thread's byte reception is interrupt-driven with a short spin-poll fast path: a streaming packet is consumed at full speed, and when the next byte is not yet there the engine arms the receive interrupt and sleeps, so the session stays correct at any transport pace (an arbitrarily large `SERIAL_PTY_POLLCYCLES`, or an arbitrarily slow real UART). If the serial interrupt is misrouted (a wrong `_gdbstub_irq`) or not wired up, the engine degrades to a ~50ms insurance timeout per wait — attach still works but crawls; fix the interrupt index rather than live with it.

**Code-write coherency** — breakpoints are self-modifying code: the dcache is write-back while `fence.i` only invalidates the icache, so the stub pushes every modified word to the memory-side coherency point with an atomic access (atomics bypass the dcache) before `fence.i`; both gdb's own breakpoint writes (`M` packets) and the stepper's temporary `ebreak` go through this path.

**Limitations**

- Single-CPU scope (`CPU_COUNT=1`); on more CPUs the stub degrades to whole-machine emergency sessions rather than per-thread parking.
- At most 16 threads (the debug-shell included) can be stub-parked at once — queued stops, `Ctrl-C` parks and step-suppressed holds share the slots. A thread stopping with the list full drops into the frozen emergency session instead, and `Ctrl-C` degrades to report-only; drain with `__gdbstub_next()`/`detach` to free slots.
- A breakpoint deleted while a queued (unreported) hit of it exists reports a spurious SIGTRAP when that stop is drained — just `continue`.
- An inferior call that blocks wedges its host thread until `Ctrl-C`; the engine stays alive throughout.
- Threads are resumed one at a time (the selection); `detach` resumes everything the stub stopped.
- A breakpoint placed inside an `lr.w`/`sc.w` sequence livelocks it: every hit kills the reservation, hence the `sc.w` can never succeed until the breakpoint is deleted (single-stepping is safe: the stepper jumps the sequence as a whole).
- Writing code whose dcache line also holds data the program dirties can lose a breakpoint on eviction.
- If gdb dies without detaching, its planted `ebreak` remain: the next hit parks that thread awaiting re-attach (attach then drains it as usual).
