#if 0
#pragma makedep unix
#pragma makedep do_not_sanitize
#endif

#if WINE_ASAN
#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "wine/debug.h"

#define NORET __attribute__((noreturn))
#define ASANAPI __attribute__((visibility("default")))

static FORCEINLINE NORET void asan_abort(void)
{
    exit(2);
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
    return STATUS_SUCCESS;
}
#endif
