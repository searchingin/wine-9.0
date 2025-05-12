#if 0
#pragma makedep unix
#pragma makedep do_not_sanitize
#endif

#if WINE_ASAN
#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "unix_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(asan_unix);

#define NORET __attribute__((noreturn))
#define ASANAPI __attribute__((visibility("default")))

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

static inline ULONG_PTR *asan_get_sp(void)
{
    ULONG_PTR *base = NtCurrentTeb() ? (ULONG_PTR *)(&NtCurrentTeb()->GdiTebBatch + 1) : NULL;
    return base ? &base[-4] : NULL;
}

static inline ULONG_PTR asan_get_stack_base(void)
{
    return (ULONG_PTR)ntdll_get_thread_data()->kernel_stack + kernel_stack_size;
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
    state->unpoison_stack = 1;
    if (NtCurrentTeb()->WowTebOffset) state->unpoison_stack_wow64 = 1;
}

/* ================ End of Stubs ================*/

/* Initialize process-wide ASan states */
void wine_asan_init(void)
{
    /* TODO: parse ASAN_OPTIONS */
}

/* Create thread specific ASan states */
NTSTATUS wine_asan_alloc_thread(TEB *teb, SIZE_T user_stack_size)
{
    BOOL is_wow64 = teb->WowTebOffset != 0;
    void **thread_data = (void **)(&teb->GdiTebBatch + 1);
    struct asan_thread_state *state = NULL;
    ULONG_PTR limit = 0;
    NTSTATUS status;
    SIZE_T size;

    TRACE("%p %zu %p\n", teb, user_stack_size, thread_data);

#ifdef _WIN64
    if (is_wow64) /* WoW64 process, allocate below 4G */
        limit = limit_4g;
#endif

    size = page_size; /* Extra page for shared states */
    status = NtAllocateVirtualMemory(NtCurrentProcess(), (void **)&state, limit, &size,
                                     MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (status)
    {
        ERR("Failed to allocate memory for PE fake stacks and shared states: %08x\n", status);
        return STATUS_NO_MEMORY;
    }

    TRACE("state: %p\n", state);

    /* Initialize metadata, and write the pointers to the end of `GdiTebBatch` block. `GdiTebBatch`
     * is used for ntdll_thread_data, which has plenty rooms left. */

    thread_data[-1] = state;
    thread_data[-4] = thread_data[-5] = NULL; /* saved stack pointers */

    if (is_wow64)
    {
        UINT32 *thread_data32 = (UINT32 *)(&get_wow_teb(teb)->GdiTebBatch + 1);

        thread_data32[-1] = (UINT32)(ULONG_PTR)state;
        thread_data32[-4] = thread_data32[-5] = 0;
    }

    return STATUS_SUCCESS;
}
#endif
