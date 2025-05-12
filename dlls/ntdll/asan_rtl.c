#if 0
#pragma makedep do_not_sanitize
#endif

#if WINE_ASAN
#include <stdarg.h>
#include "winternl.h"
#include "asan_private.h"
#include "wine/asan_interface.h"

#define NORET __attribute__((noreturn))
#define ASANAPI

ULONG_PTR __asan_shadow_memory_dynamic_address = ASAN_SHADOW_OFFSET;
int __asan_option_detect_stack_use_after_return = 1;

static FORCEINLINE NORET void asan_abort(void)
{
    RtlExitUserProcess(2);
}

#define REAL(func) __wine_asan_real_##func
#include "asan_load_store.h"
#include "asan_fake_stack.h"

struct __asan_global;
/* Functions concerning instrumented global variables */
void __asan_register_image_globals(ULONG_PTR *flag)
{
}

void __asan_unregister_image_globals(ULONG_PTR *flag)
{
}

void __asan_register_elf_globals(ULONG_PTR *flag, void *start, void *stop)
{
}

void __asan_unregister_elf_globals(ULONG_PTR *flag, void *start, void *stop)
{
}

void __asan_register_globals(void *globals, ULONG_PTR n)
{
}

void __asan_unregister_globals(void *globals, ULONG_PTR n)
{
}

/* Functions concerning dynamic library initialization */
void __asan_before_dynamic_init(const void *name)
{
}

void __asan_after_dynamic_init(void)
{
}

/* On the PE side, these functions are naturally instrumented since their code is
 * in msvcrt, which will be compiled with ASan. Albeit being slower. */
void *__asan_memcpy(void *dst, const void *src, ULONG_PTR size)
{
    return memcpy(dst, src, size);
}

void *__asan_memset(void *s, int c, ULONG_PTR n)
{
    return memset(s, c, n);
}

void *__asan_memmove(void *dest, const void *src, ULONG_PTR n)
{
    return memmove(dest, src, n);
}

void __asan_version_mismatch_check_v8(void)
{
}

void __asan_handle_no_return(void)
{
}

void __asan_init(void)
{
}

ULONG_PTR __asan_get_shadow_memory_dynamic_address(void)
{
    return __asan_shadow_memory_dynamic_address;
}

int __asan_should_detect_stack_use_after_return(void)
{
    return __asan_option_detect_stack_use_after_return;
}

// Functions concerning redzone poisoning
void __asan_poison_intra_object_redzone(ULONG_PTR p, ULONG_PTR size)
{
}

void __asan_unpoison_intra_object_redzone(ULONG_PTR p, ULONG_PTR size)
{
}

// Functions concerning array cookie poisoning
void __asan_poison_cxx_array_cookie(ULONG_PTR p)
{
}

ULONG_PTR __asan_load_cxx_array_cookie(ULONG_PTR *p)
{
    return *p;
}

// Functions concerning fake stacks
void *__asan_get_current_fake_stack(void)
{
    return NULL;
}

void *__asan_addr_is_in_fake_stack(void *fake_stack, void *addr, void **beg, void **end)
{
    return NULL;
}

// Functions concerning poisoning and unpoisoning fake stack alloca
void __asan_alloca_poison(void *addr, SIZE_T size)
{
}

void __asan_allocas_unpoison(void *top, SIZE_T bottom)
{
}

void wine_asan_init(void)
{
}
#endif
