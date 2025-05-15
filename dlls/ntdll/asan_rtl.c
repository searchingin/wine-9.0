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
static const ULONG_PTR page_mask = 0xfff;

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

/* clang-format off */
static struct wine_asan_funcs fns =
{
    __asan_poison_memory_region,
    __asan_unpoison_memory_region,
    __asan_poison_stack_memory,
    __asan_unpoison_stack_memory,
    __asan_address_is_poisoned,
    __asan_region_is_poisoned,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    __asan_get_current_fake_stack,
    __asan_addr_is_in_fake_stack,
    __asan_handle_no_return,
    NULL,

    __asan_init,
    __asan_get_shadow_memory_dynamic_address,
    __asan_should_detect_stack_use_after_return,
    __asan_version_mismatch_check_v8,
    __asan_register_image_globals,
    __asan_unregister_image_globals,
    __asan_register_elf_globals,
    __asan_unregister_elf_globals,
    __asan_register_globals,
    __asan_unregister_globals,
    __asan_before_dynamic_init,
    __asan_after_dynamic_init,
    __asan_set_shadow_00,
    __asan_set_shadow_01,
    __asan_set_shadow_02,
    __asan_set_shadow_03,
    __asan_set_shadow_04,
    __asan_set_shadow_05,
    __asan_set_shadow_06,
    __asan_set_shadow_07,
    __asan_set_shadow_f1,
    __asan_set_shadow_f2,
    __asan_set_shadow_f3,
    __asan_set_shadow_f5,
    __asan_set_shadow_f8,
    __asan_stack_free_0,
    __asan_stack_free_1,
    __asan_stack_free_2,
    __asan_stack_free_3,
    __asan_stack_free_4,
    __asan_stack_free_5,
    __asan_stack_free_6,
    __asan_stack_free_7,
    __asan_stack_free_8,
    __asan_stack_free_9,
    __asan_stack_free_10,
    __asan_stack_malloc_0,
    __asan_stack_malloc_1,
    __asan_stack_malloc_2,
    __asan_stack_malloc_3,
    __asan_stack_malloc_4,
    __asan_stack_malloc_5,
    __asan_stack_malloc_6,
    __asan_stack_malloc_7,
    __asan_stack_malloc_8,
    __asan_stack_malloc_9,
    __asan_stack_malloc_10,
    __asan_stack_malloc_always_0,
    __asan_stack_malloc_always_1,
    __asan_stack_malloc_always_2,
    __asan_stack_malloc_always_3,
    __asan_stack_malloc_always_4,
    __asan_stack_malloc_always_5,
    __asan_stack_malloc_always_6,
    __asan_stack_malloc_always_7,
    __asan_stack_malloc_always_8,
    __asan_stack_malloc_always_9,
    __asan_stack_malloc_always_10,
    __asan_load1,
    __asan_load2,
    __asan_load4,
    __asan_load8,
    __asan_load16,
    __asan_store1,
    __asan_store2,
    __asan_store4,
    __asan_store8,
    __asan_store16,
    __asan_loadN,
    __asan_storeN,
    __asan_load1_noabort,
    __asan_load2_noabort,
    __asan_load4_noabort,
    __asan_load8_noabort,
    __asan_load16_noabort,
    __asan_store1_noabort,
    __asan_store2_noabort,
    __asan_store4_noabort,
    __asan_store8_noabort,
    __asan_store16_noabort,
    __asan_loadN_noabort,
    __asan_storeN_noabort,
    __asan_report_load1,
    __asan_report_load2,
    __asan_report_load4,
    __asan_report_load8,
    __asan_report_load16,
    __asan_report_store1,
    __asan_report_store2,
    __asan_report_store4,
    __asan_report_store8,
    __asan_report_store16,
    __asan_report_load_n,
    __asan_report_store_n,
    __asan_report_load1_noabort,
    __asan_report_load2_noabort,
    __asan_report_load4_noabort,
    __asan_report_load8_noabort,
    __asan_report_load16_noabort,
    __asan_report_store1_noabort,
    __asan_report_store2_noabort,
    __asan_report_store4_noabort,
    __asan_report_store8_noabort,
    __asan_report_store16_noabort,
    __asan_report_load_n_noabort,
    __asan_report_store_n_noabort,
    __asan_exp_load1,
    __asan_exp_load2,
    __asan_exp_load4,
    __asan_exp_load8,
    __asan_exp_load16,
    __asan_exp_store1,
    __asan_exp_store2,
    __asan_exp_store4,
    __asan_exp_store8,
    __asan_exp_store16,
    __asan_exp_loadN,
    __asan_exp_storeN,
    __asan_memcpy,
    __asan_memset,
    __asan_memmove,
    __asan_poison_cxx_array_cookie,
    __asan_load_cxx_array_cookie,
    __asan_poison_intra_object_redzone,
    __asan_unpoison_intra_object_redzone,
    __asan_alloca_poison,
    __asan_allocas_unpoison,
    NULL,
    NULL,
};
/* clang-format on */

void wine_asan_init(void)
{
    struct wine_asan_funcs **pfns = (void *)(((ULONG_PTR)NtCurrentTeb()->Peb & ~page_mask) + page_mask + 1);
    pfns[-1] = &fns;
}

#endif
