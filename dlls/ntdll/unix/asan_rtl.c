#if 0
#pragma makedep unix
#pragma makedep do_not_sanitize
#endif

#if WINE_ASAN
#include <stdlib.h>
#include <dlfcn.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "unix_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(asan_unix);

#define PROCS_NOASAN                                                                               \
    X(memset, void *, void *s, int c, size_t n)

#define NORET __attribute__((noreturn))
#define ASANAPI __attribute__((visibility("default")))
#ifdef __clang__
#define NOBUILTIN __attribute__((no_builtin))
#else
#define NOBUILTIN __attribute__((optimize("inline-stringops")))
#endif

#define X(func, r, ...) r (*__wine_asan_real_##func)(__VA_ARGS__) = NULL;
PROCS_NOASAN
#undef X

#define ALIGNUP(s, a) (((s) + (a) - 1) & ~((a) - 1))

static FORCEINLINE NORET void asan_abort(void)
{
    exit(2);
}

static inline struct asan_thread_state *asan_get_thread_state(void)
{
    /* The `GdiTebBatch` field is the first field in the TEB structure, so we can use it to get the
     * pointer to the `struct asan_thread_state`. */
    void **base;

    if (!NtCurrentTeb()) return NULL;
    base = (void **)(&NtCurrentTeb()->GdiTebBatch + 1);
    return base[-1];
}

struct asan_fake_stack;
static inline struct asan_fake_stack *asan_get_fake_stack(void)
{
    void **base;

    if (!NtCurrentTeb()) return NULL;
    /* If we are currently in sigaltstack, don't use fake stacks. Because our signal handlers assume
     * that the stack they are on is the signal stack, using fake stacks breaks that assumption. */
    if (is_inside_signal_stack(__builtin_frame_address(0))) return NULL;
    base = (void **)(&NtCurrentTeb()->GdiTebBatch + 1);
    return base[-2];
}

static inline ULONG_PTR *asan_get_sp(void)
{
    ULONG_PTR *base = NtCurrentTeb() ? (ULONG_PTR *)(&NtCurrentTeb()->GdiTebBatch + 1) : NULL;
    return base ? &base[-4] : NULL;
}

static inline ULONG_PTR asan_get_stack_base(void)
{
    return (ULONG_PTR)ntdll_get_thread_data()->kernel_stack + kernel_stack_size;
}

static inline BOOL asan_state_check_and_reset_gc(void)
{
    struct asan_thread_state *state = asan_get_thread_state();
    if (!state) return FALSE;
    if (state->gc_unix)
    {
        state->gc_unix = 0;
        return TRUE;
    }
    return FALSE;
}

static inline BOOL asan_state_check_and_reset_unpoison_stack(void)
{
    struct asan_thread_state *state = asan_get_thread_state();
    if (!state) return FALSE;
    if (state->unpoison_stack_unix)
    {
        state->unpoison_stack_unix = 0;
        return TRUE;
    }
    return FALSE;
}

#define REAL(func) __wine_asan_real_##func
#include "../asan_load_store.h"
#include "../asan_fake_stack.h"

/* Right now we only support ASan with `detect_stack_use_after_return` enabled. This option enables
 * ASan to use fake stacks, which makes stack switching easier for us to handle. */
ULONG_PTR ASANAPI __asan_shadow_memory_dynamic_address = ASAN_SHADOW_OFFSET;
int ASANAPI __asan_option_detect_stack_use_after_return = 1;

void ASANAPI __asan_init(void)
{
    /* ASan initialization should've been done when ntdll is loaded, so
     * nothing to do here */
}

/* ================ Stubs ================*/
struct __asan_global;
void ASANAPI __asan_register_image_globals(ULONG_PTR *flag)
{
}

void ASANAPI __asan_unregister_image_globals(ULONG_PTR *flag)
{
}

void ASANAPI __asan_register_elf_globals(ULONG_PTR *flag, void *start, void *stop)
{
}

void ASANAPI __asan_unregister_elf_globals(ULONG_PTR *flag, void *start, void *stop)
{
}

void ASANAPI __asan_register_globals(void *globals, ULONG_PTR n)
{
}

void ASANAPI __asan_unregister_globals(void *globals, ULONG_PTR n)
{
}

void ASANAPI __asan_before_dynamic_init(const void *module_name)
{
}

void ASANAPI __asan_after_dynamic_init(void)
{
}

void ASANAPI *__asan_memcpy(void *dst, const void *src, ULONG_PTR size)
{
    return memcpy(dst, src, size);
}

void ASANAPI *__asan_memset(void *s, int c, ULONG_PTR n)
{
    return memset(s, c, n);
}

void ASANAPI *__asan_memmove(void *dest, const void *src, ULONG_PTR n)
{
    return memmove(dest, src, n);
}

void ASANAPI __asan_version_mismatch_check_v8(void)
{
}

void ASANAPI __asan_handle_no_return(void)
{
    struct asan_thread_state *state = asan_get_thread_state();
    void *rsp = __builtin_frame_address(0);
    struct ntdll_thread_data *thread_data;

    /* noreturn called before we have a Teb, this happens if `check_command_line` decides to reexec,
     * not unpoisoning the stack should be fine. */
    if (!state) return;

    thread_data = ntdll_get_thread_data();

    if (is_inside_signal_stack(rsp))
    {
        __asan_unpoison_stack_memory(get_signal_stack(), signal_stack_size);
        __asan_unpoison_stack_memory(thread_data->kernel_stack, kernel_stack_size);
    }
    else if ((ULONG_PTR)rsp >= (ULONG_PTR)thread_data->kernel_stack &&
             (ULONG_PTR)rsp < (ULONG_PTR)thread_data->kernel_stack + kernel_stack_size)
        __asan_unpoison_stack_memory(rsp,
                (ULONG_PTR)thread_data->kernel_stack + kernel_stack_size - (ULONG_PTR)rsp);

    if (!state) return;
    /* schedule garbage collection for fake stacks. */
    state->gc_unix = state->gc = 1;
    state->unpoison_stack = 1;
    if (NtCurrentTeb()->WowTebOffset) state->unpoison_stack_wow64 = 1;
}

/* ================ End of Stubs ================*/

/* Initialize process-wide ASan states */
void wine_asan_init(void)
{
#define X(func, ...) __wine_asan_real_##func = (void *)dlsym(RTLD_NEXT, #func);
    PROCS_NOASAN
#undef X
    /* TODO: parse ASAN_OPTIONS */
}

/* Create shared and unix side ASan state for the current thread *
 * Currently, this contains a `asan_thread_state` and the memory pool for fake stacks.
 * `asan_thread_state` is shared between PE and Unix, so we need to make sure it's mapped below 4G
 * in case we are in WoW64. */
NTSTATUS wine_asan_alloc_thread(TEB *teb, SIZE_T user_stack_size)
{
    BOOL is_wow64 = teb->WowTebOffset != 0;
    void **thread_data = (void **)(&teb->GdiTebBatch + 1);
    void *pe_fake_stacks;
    struct asan_thread_state *state = NULL;
    ULONG_PTR limit = 0;
    NTSTATUS status;
    struct asan_fake_stack *fake_stacks = NULL;
    SIZE_T max_user_stack_shift = is_wow64 ? ASAN_FAKE_STACK_MAX_SIZE_SHIFT_32BIT : ASAN_FAKE_STACK_MAX_SIZE_SHIFT;
    SIZE_T size, user_stack_shift = 0;

    TRACE("%p %zu %p\n", teb, user_stack_size, thread_data);

#ifdef _WIN64
    if (is_wow64) /* WoW64 process, allocate below 4G */
        limit = limit_4g;
#endif

    size = asan_fake_stack_total_size(kernel_stack_shift, page_size);
    status = NtAllocateVirtualMemory(NtCurrentProcess(), (void **)&fake_stacks, 0, &size,
                                     MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (status)
    {
        ERR("Failed to allocate memory for unix fake stacks: %08x\n", status);
        return STATUS_NO_MEMORY;
    }
    TRACE("fake stack: %p\n", fake_stacks);

    /* Now, allocate the initial set of fake stacks for PE. When new Fibers are created on the PE
     * side, more sets might be allocated. The ASan thread states are allocated together */
    while (user_stack_size > (1 << user_stack_shift)) user_stack_shift++;
    if (user_stack_shift > max_user_stack_shift)
        user_stack_shift = max_user_stack_shift;
    if (user_stack_shift < ASAN_FAKE_STACK_MIN_SIZE_SHIFT)
        user_stack_shift = ASAN_FAKE_STACK_MIN_SIZE_SHIFT;
    size = asan_fake_stack_total_size(user_stack_shift, page_size);
    size = size + page_size; /* Extra page for shared states */
    status = NtAllocateVirtualMemory(NtCurrentProcess(), (void **)&state, limit, &size,
                                     MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (status)
    {
        ERR("Failed to allocate memory for PE fake stacks and shared states: %08x\n", status);
        NtFreeVirtualMemory(NtCurrentProcess(), (void **)&fake_stacks, 0, MEM_RELEASE);
        return STATUS_NO_MEMORY;
    }

    TRACE("state: %p\n", state);

    /* Only start setting pointers to fake stacks after we have allocated everything. Since after
     * this point we are not calling any other functions, we can be sure __asan_stack_malloc* won't
     * be called with partially initialized fake stack states. */
    /* Initialize fake stack metadata, and write the pointers to the end of `GdiTebBatch` block.
     * `GdiTebBatch` is used for ntdll_thread_data, which has plenty rooms left. */

    pe_fake_stacks = (void *)((ULONG_PTR)state + page_size);
    fake_stacks->page_size = page_size;
    fake_stacks->stack_shift = kernel_stack_shift;
    thread_data[-1] = state;
    thread_data[-2] = fake_stacks;
    thread_data[-4] = thread_data[-5] = NULL; /* saved stack pointers */

    if (is_wow64)
    {
        struct
        {
            UINT32 page_size;
            UINT32 stack_shift;
            UINT32 allocation_head[ASAN_FAKE_STACK_NUM_OF_SIZE_CLASSES];
        } *fake_stacks32 = pe_fake_stacks;
        UINT32 *thread_data32 = (UINT32 *)(&get_wow_teb(teb)->GdiTebBatch + 1);

        fake_stacks32->page_size = page_size;
        fake_stacks32->stack_shift = user_stack_shift;

        thread_data32[-1] = (UINT32)(ULONG_PTR)state;
        thread_data32[-2] = 0; /* 32-bit PE code shouldn't see 64-bit unix fake stacks */
        thread_data32[-3] = (UINT32)(ULONG_PTR)pe_fake_stacks;
        thread_data32[-4] = thread_data32[-5] = 0;
        thread_data[-3] = NULL; /* 64-bit PE code doesn't use fake stacks */
    }
    else
    {
        fake_stacks = pe_fake_stacks;
        fake_stacks->page_size = page_size;
        fake_stacks->stack_shift = user_stack_shift;
        thread_data[-3] = fake_stacks;
    }

    return STATUS_SUCCESS;
}

#endif
