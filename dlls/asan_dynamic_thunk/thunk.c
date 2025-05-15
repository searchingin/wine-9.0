#if 0
#pragma makedep do_not_sanitize
#endif

#define WINE_ASAN 1
#include <stdarg.h>
#include "windef.h"
#include "winternl.h"
#include "wine/asan_interface.h"

static struct wine_asan_funcs fns;
static LONG wine_asan_is_initialized = 0; /* bit 0 - started, bit 1 - completed */
static const ULONG_PTR page_mask = 0xfff;

/* ASan instrumentation relies on a global variable to store the
 * address of the shadow memory. This variable has to be initialized
 * when a DLL is loaded. On windows this is done by putting an init
 * function into `.CRT$XIB` which is called by the CRT at load time.
 * wine doesn't implement this functionality, therefore we do it
 * directly here. */
ULONG_PTR __asan_shadow_memory_dynamic_address = 0;
int __asan_option_detect_stack_use_after_return = 1;

void init_asan_dynamic_thunk(void)
{
    struct wine_asan_funcs **pfns = (void *)(((ULONG_PTR)NtCurrentTeb()->Peb & ~page_mask) + page_mask + 1);
    if (InterlockedOr(&wine_asan_is_initialized, 1))
    {
        while (InterlockedOr(&wine_asan_is_initialized, 0) != 3);
        return;
    }
    fns = *pfns[-1];
    __asan_shadow_memory_dynamic_address = __asan_get_shadow_memory_dynamic_address();
    __asan_option_detect_stack_use_after_return = __asan_should_detect_stack_use_after_return();
    InterlockedOr(&wine_asan_is_initialized, 2);
}

#define DEFINE_ASAN_THUNKS(prefix, init)                                                                         \
    void prefix##__asan_poison_memory_region(void const volatile *addr, SIZE_T size)                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_poison_memory_region(addr, size);                                                           \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_unpoison_memory_region(void const volatile *addr, SIZE_T size)                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_unpoison_memory_region(addr, size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_poison_stack_memory(void const volatile *addr, SIZE_T size)                              \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_poison_stack_memory(addr, size);                                                            \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_unpoison_stack_memory(void const volatile *addr, SIZE_T size)                            \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_unpoison_stack_memory(addr, size);                                                          \
    }                                                                                                            \
                                                                                                                 \
    int prefix##__asan_address_is_poisoned(void const volatile *addr)                                            \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_address_is_poisoned(addr);                                                           \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_region_is_poisoned(void *beg, SIZE_T size)                                              \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_region_is_poisoned(beg, size);                                                       \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_describe_address(void *addr)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_describe_address(addr);                                                                     \
    }                                                                                                            \
                                                                                                                 \
    int prefix##__asan_report_present(void)                                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_report_present();                                                                    \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_get_report_pc(void)                                                                     \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_report_pc();                                                                     \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_get_report_bp(void)                                                                     \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_report_bp();                                                                     \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_get_report_sp(void)                                                                     \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_report_sp();                                                                     \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_get_report_address(void)                                                                \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_report_address();                                                                \
    }                                                                                                            \
                                                                                                                 \
    int prefix##__asan_get_report_access_type(void)                                                              \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_report_access_type();                                                            \
    }                                                                                                            \
                                                                                                                 \
    SIZE_T prefix##__asan_get_report_access_size(void)                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_report_access_size();                                                            \
    }                                                                                                            \
                                                                                                                 \
    const char *prefix##__asan_get_report_description(void)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_report_description();                                                            \
    }                                                                                                            \
                                                                                                                 \
    const char *prefix##__asan_locate_address(void *addr, char *name, SIZE_T name_size,                          \
            void **region_address, SIZE_T *region_size)                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_locate_address(addr, name, name_size, region_address, region_size);                  \
    }                                                                                                            \
                                                                                                                 \
    SIZE_T prefix##__asan_get_alloc_stack(void *addr, void **trace, SIZE_T size, int *thread_id)                 \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_alloc_stack(addr, trace, size, thread_id);                                       \
    }                                                                                                            \
                                                                                                                 \
    SIZE_T prefix##__asan_get_free_stack(void *addr, void **trace, SIZE_T size, int *thread_id)                  \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_free_stack(addr, trace, size, thread_id);                                        \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_get_shadow_mapping(SIZE_T *shadow_scale, SIZE_T *shadow_offset)                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_get_shadow_mapping(shadow_scale, shadow_offset);                                            \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_error(void *pc, void *bp, void *sp, void *addr, int is_write, SIZE_T access_size) \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_error(pc, bp, sp, addr, is_write, access_size);                                      \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_death_callback(void (*callback)(void))                                               \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_death_callback(callback);                                                               \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_error_report_callback(void (*callback)(const char *))                                \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_error_report_callback(callback);                                                        \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_on_error(void)                                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_on_error();                                                                                 \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_print_accumulated_stats(void)                                                            \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_print_accumulated_stats();                                                                  \
    }                                                                                                            \
                                                                                                                 \
    const char *prefix##__asan_default_options(void)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_default_options();                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_get_current_fake_stack(void)                                                            \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_current_fake_stack();                                                            \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_addr_is_in_fake_stack(void *fake_stack, void *addr, void **beg, void **end)             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_addr_is_in_fake_stack(fake_stack, addr, beg, end);                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_handle_no_return(void)                                                                   \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_handle_no_return();                                                                         \
    }                                                                                                            \
                                                                                                                 \
    int prefix##__asan_update_allocation_context(void *addr)                                                     \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_update_allocation_context(addr);                                                     \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_init(void)                                                                               \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_init();                                                                                     \
    }                                                                                                            \
                                                                                                                 \
    ULONG_PTR prefix##__asan_get_shadow_memory_dynamic_address(void)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_get_shadow_memory_dynamic_address();                                                 \
    }                                                                                                            \
                                                                                                                 \
    int prefix##__asan_should_detect_stack_use_after_return(void)                                                \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_should_detect_stack_use_after_return();                                              \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_version_mismatch_check_v8(void)                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_version_mismatch_check_v8();                                                                \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_register_image_globals(ULONG_PTR *flag)                                                  \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_register_image_globals(flag);                                                               \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_unregister_image_globals(ULONG_PTR *flag)                                                \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_unregister_image_globals(flag);                                                             \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_register_elf_globals(ULONG_PTR *flag, void *start, void *stop)                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_register_elf_globals(flag, start, stop);                                                    \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_unregister_elf_globals(ULONG_PTR *flag, void *start, void *stop)                         \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_unregister_elf_globals(flag, start, stop);                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_register_globals(void *globals, ULONG_PTR n)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_register_globals(globals, n);                                                               \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_unregister_globals(void *globals, ULONG_PTR n)                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_unregister_globals(globals, n);                                                             \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_before_dynamic_init(const void *name)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_before_dynamic_init(name);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_after_dynamic_init(void)                                                                 \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_after_dynamic_init();                                                                       \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_00(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_00(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_01(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_01(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_02(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_02(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_03(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_03(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_04(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_04(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_05(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_05(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_06(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_06(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_07(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_07(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_f1(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_f1(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_f2(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_f2(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_f3(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_f3(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_f5(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_f5(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_set_shadow_f8(const void *addr, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_set_shadow_f8(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_0(void *addr, SIZE_T size)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_0(addr, size);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_1(void *addr, SIZE_T size)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_1(addr, size);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_2(void *addr, SIZE_T size)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_2(addr, size);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_3(void *addr, SIZE_T size)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_3(addr, size);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_4(void *addr, SIZE_T size)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_4(addr, size);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_5(void *addr, SIZE_T size)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_5(addr, size);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_6(void *addr, SIZE_T size)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_6(addr, size);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_7(void *addr, SIZE_T size)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_7(addr, size);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_8(void *addr, SIZE_T size)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_8(addr, size);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_9(void *addr, SIZE_T size)                                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_9(addr, size);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_stack_free_10(void *addr, SIZE_T size)                                                   \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_stack_free_10(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_0(SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_0(size);                                                                \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_1(SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_1(size);                                                                \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_2(SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_2(size);                                                                \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_3(SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_3(size);                                                                \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_4(SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_4(size);                                                                \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_5(SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_5(size);                                                                \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_6(SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_6(size);                                                                \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_7(SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_7(size);                                                                \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_8(SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_8(size);                                                                \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_9(SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_9(size);                                                                \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_10(SIZE_T size)                                                            \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_10(size);                                                               \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_0(SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_0(size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_1(SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_1(size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_2(SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_2(size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_3(SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_3(size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_4(SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_4(size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_5(SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_5(size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_6(SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_6(size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_7(SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_7(size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_8(SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_8(size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_9(SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_9(size);                                                         \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_stack_malloc_always_10(SIZE_T size)                                                     \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_stack_malloc_always_10(size);                                                        \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_load1(void *p)                                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_load1(p);                                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_load2(void *p)                                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_load2(p);                                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_load4(void *p)                                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_load4(p);                                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_load8(void *p)                                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_load8(p);                                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_load16(void *p)                                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_load16(p);                                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_store1(void *p)                                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_store1(p);                                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_store2(void *p)                                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_store2(p);                                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_store4(void *p)                                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_store4(p);                                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_store8(void *p)                                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_store8(p);                                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_store16(void *p)                                                                         \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_store16(p);                                                                                 \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_loadN(void *p, SIZE_T size)                                                              \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_loadN(p, size);                                                                             \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_storeN(void *p, SIZE_T size)                                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_storeN(p, size);                                                                            \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_load1_noabort(void *p)                                                                   \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_load1_noabort(p);                                                                           \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_load2_noabort(void *p)                                                                   \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_load2_noabort(p);                                                                           \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_load4_noabort(void *p)                                                                   \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_load4_noabort(p);                                                                           \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_load8_noabort(void *p)                                                                   \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_load8_noabort(p);                                                                           \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_load16_noabort(void *p)                                                                  \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_load16_noabort(p);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_store1_noabort(void *p)                                                                  \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_store1_noabort(p);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_store2_noabort(void *p)                                                                  \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_store2_noabort(p);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_store4_noabort(void *p)                                                                  \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_store4_noabort(p);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_store8_noabort(void *p)                                                                  \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_store8_noabort(p);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_store16_noabort(void *p)                                                                 \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_store16_noabort(p);                                                                         \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_loadN_noabort(void *p, SIZE_T size)                                                      \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_loadN_noabort(p, size);                                                                     \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_storeN_noabort(void *p, SIZE_T size)                                                     \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_storeN_noabort(p, size);                                                                    \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_load1(void *p)                                                  \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load1(p);                                                                            \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_load2(void *p)                                                  \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load2(p);                                                                            \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_load4(void *p)                                                  \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load4(p);                                                                            \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_load8(void *p)                                                  \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load8(p);                                                                            \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_load16(void *p)                                                 \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load16(p);                                                                           \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_store1(void *p)                                                 \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store1(p);                                                                           \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_store2(void *p)                                                 \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store2(p);                                                                           \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_store4(void *p)                                                 \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store4(p);                                                                           \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_store8(void *p)                                                 \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store8(p);                                                                           \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_store16(void *p)                                                \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store16(p);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_load_n(void *p, SIZE_T size)                                    \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load_n(p, size);                                                                     \
    }                                                                                                            \
                                                                                                                 \
    void DECLSPEC_NORETURN prefix##__asan_report_store_n(void *p, SIZE_T size)                                   \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store_n(p, size);                                                                    \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_load1_noabort(void *p)                                                            \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load1_noabort(p);                                                                    \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_load2_noabort(void *p)                                                            \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load2_noabort(p);                                                                    \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_load4_noabort(void *p)                                                            \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load4_noabort(p);                                                                    \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_load8_noabort(void *p)                                                            \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load8_noabort(p);                                                                    \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_load16_noabort(void *p)                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load16_noabort(p);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_store1_noabort(void *p)                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store1_noabort(p);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_store2_noabort(void *p)                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store2_noabort(p);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_store4_noabort(void *p)                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store4_noabort(p);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_store8_noabort(void *p)                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store8_noabort(p);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_store16_noabort(void *p)                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store16_noabort(p);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_load_n_noabort(void *p, SIZE_T size)                                              \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_load_n_noabort(p, size);                                                             \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_report_store_n_noabort(void *p, SIZE_T size)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_report_store_n_noabort(p, size);                                                            \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_load1(void *p, UINT32 exp)                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_load1(p, exp);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_load2(void *p, UINT32 exp)                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_load2(p, exp);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_load4(void *p, UINT32 exp)                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_load4(p, exp);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_load8(void *p, UINT32 exp)                                                           \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_load8(p, exp);                                                                          \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_load16(void *p, UINT32 exp)                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_load16(p, exp);                                                                         \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_store1(void *p, UINT32 exp)                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_store1(p, exp);                                                                         \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_store2(void *p, UINT32 exp)                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_store2(p, exp);                                                                         \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_store4(void *p, UINT32 exp)                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_store4(p, exp);                                                                         \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_store8(void *p, UINT32 exp)                                                          \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_store8(p, exp);                                                                         \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_store16(void *p, UINT32 exp)                                                         \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_store16(p, exp);                                                                        \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_loadN(void *p, SIZE_T size, UINT32 exp)                                              \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_loadN(p, size, exp);                                                                    \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_exp_storeN(void *p, SIZE_T size, UINT32 exp)                                             \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_exp_storeN(p, size, exp);                                                                   \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_memcpy(void *dst, const void *src, ULONG_PTR size)                                      \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_memcpy(dst, src, size);                                                              \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_memset(void *s, int c, ULONG_PTR n)                                                     \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_memset(s, c, n);                                                                     \
    }                                                                                                            \
                                                                                                                 \
    void *prefix##__asan_memmove(void *dest, const void *src, ULONG_PTR n)                                       \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_memmove(dest, src, n);                                                               \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_poison_cxx_array_cookie(ULONG_PTR p)                                                     \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_poison_cxx_array_cookie(p);                                                                 \
    }                                                                                                            \
                                                                                                                 \
    ULONG_PTR prefix##__asan_load_cxx_array_cookie(ULONG_PTR *p)                                                 \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_load_cxx_array_cookie(p);                                                            \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_poison_intra_object_redzone(ULONG_PTR p, ULONG_PTR size)                                 \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_poison_intra_object_redzone(p, size);                                                       \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_unpoison_intra_object_redzone(ULONG_PTR p, ULONG_PTR size)                               \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_unpoison_intra_object_redzone(p, size);                                                     \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_alloca_poison(void *addr, SIZE_T size)                                                   \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_alloca_poison(addr, size);                                                                  \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_allocas_unpoison(void *top, SIZE_T bottom)                                               \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_allocas_unpoison(top, bottom);                                                              \
    }                                                                                                            \
                                                                                                                 \
    const char *prefix##__asan_default_suppressions(void)                                                        \
    {                                                                                                            \
        init;                                                                                                    \
        return fns.p___asan_default_suppressions();                                                              \
    }                                                                                                            \
                                                                                                                 \
    void prefix##__asan_handle_vfork(void *sp)                                                                   \
    {                                                                                                            \
        init;                                                                                                    \
        fns.p___asan_handle_vfork(sp);                                                                           \
    }

DEFINE_ASAN_THUNKS(, (void)0)
DEFINE_ASAN_THUNKS(__wine_asan_delayloader_, init_asan_dynamic_thunk())

/* clang-format off */
/* pre-fill this table with stub versions of ASan API functions that will automatically load the
 * real function pointers once called. */
static struct wine_asan_funcs fns =
{
    __wine_asan_delayloader___asan_poison_memory_region,
    __wine_asan_delayloader___asan_unpoison_memory_region,
    __wine_asan_delayloader___asan_poison_stack_memory,
    __wine_asan_delayloader___asan_unpoison_stack_memory,
    __wine_asan_delayloader___asan_address_is_poisoned,
    __wine_asan_delayloader___asan_region_is_poisoned,
    __wine_asan_delayloader___asan_describe_address,
    __wine_asan_delayloader___asan_report_present,
    __wine_asan_delayloader___asan_get_report_pc,
    __wine_asan_delayloader___asan_get_report_bp,
    __wine_asan_delayloader___asan_get_report_sp,
    __wine_asan_delayloader___asan_get_report_address,
    __wine_asan_delayloader___asan_get_report_access_type,
    __wine_asan_delayloader___asan_get_report_access_size,
    __wine_asan_delayloader___asan_get_report_description,
    __wine_asan_delayloader___asan_locate_address,
    __wine_asan_delayloader___asan_get_alloc_stack,
    __wine_asan_delayloader___asan_get_free_stack,
    __wine_asan_delayloader___asan_get_shadow_mapping,
    __wine_asan_delayloader___asan_report_error,
    __wine_asan_delayloader___asan_set_death_callback,
    __wine_asan_delayloader___asan_set_error_report_callback,
    __wine_asan_delayloader___asan_on_error,
    __wine_asan_delayloader___asan_print_accumulated_stats,
    __wine_asan_delayloader___asan_default_options,
    __wine_asan_delayloader___asan_get_current_fake_stack,
    __wine_asan_delayloader___asan_addr_is_in_fake_stack,
    __wine_asan_delayloader___asan_handle_no_return,
    __wine_asan_delayloader___asan_update_allocation_context,
    __wine_asan_delayloader___asan_init,
    __wine_asan_delayloader___asan_get_shadow_memory_dynamic_address,
    __wine_asan_delayloader___asan_should_detect_stack_use_after_return,
    __wine_asan_delayloader___asan_version_mismatch_check_v8,
    __wine_asan_delayloader___asan_register_image_globals,
    __wine_asan_delayloader___asan_unregister_image_globals,
    __wine_asan_delayloader___asan_register_elf_globals,
    __wine_asan_delayloader___asan_unregister_elf_globals,
    __wine_asan_delayloader___asan_register_globals,
    __wine_asan_delayloader___asan_unregister_globals,
    __wine_asan_delayloader___asan_before_dynamic_init,
    __wine_asan_delayloader___asan_after_dynamic_init,
    __wine_asan_delayloader___asan_set_shadow_00,
    __wine_asan_delayloader___asan_set_shadow_01,
    __wine_asan_delayloader___asan_set_shadow_02,
    __wine_asan_delayloader___asan_set_shadow_03,
    __wine_asan_delayloader___asan_set_shadow_04,
    __wine_asan_delayloader___asan_set_shadow_05,
    __wine_asan_delayloader___asan_set_shadow_06,
    __wine_asan_delayloader___asan_set_shadow_07,
    __wine_asan_delayloader___asan_set_shadow_f1,
    __wine_asan_delayloader___asan_set_shadow_f2,
    __wine_asan_delayloader___asan_set_shadow_f3,
    __wine_asan_delayloader___asan_set_shadow_f5,
    __wine_asan_delayloader___asan_set_shadow_f8,
    __wine_asan_delayloader___asan_stack_free_0,
    __wine_asan_delayloader___asan_stack_free_1,
    __wine_asan_delayloader___asan_stack_free_2,
    __wine_asan_delayloader___asan_stack_free_3,
    __wine_asan_delayloader___asan_stack_free_4,
    __wine_asan_delayloader___asan_stack_free_5,
    __wine_asan_delayloader___asan_stack_free_6,
    __wine_asan_delayloader___asan_stack_free_7,
    __wine_asan_delayloader___asan_stack_free_8,
    __wine_asan_delayloader___asan_stack_free_9,
    __wine_asan_delayloader___asan_stack_free_10,
    __wine_asan_delayloader___asan_stack_malloc_0,
    __wine_asan_delayloader___asan_stack_malloc_1,
    __wine_asan_delayloader___asan_stack_malloc_2,
    __wine_asan_delayloader___asan_stack_malloc_3,
    __wine_asan_delayloader___asan_stack_malloc_4,
    __wine_asan_delayloader___asan_stack_malloc_5,
    __wine_asan_delayloader___asan_stack_malloc_6,
    __wine_asan_delayloader___asan_stack_malloc_7,
    __wine_asan_delayloader___asan_stack_malloc_8,
    __wine_asan_delayloader___asan_stack_malloc_9,
    __wine_asan_delayloader___asan_stack_malloc_10,
    __wine_asan_delayloader___asan_stack_malloc_always_0,
    __wine_asan_delayloader___asan_stack_malloc_always_1,
    __wine_asan_delayloader___asan_stack_malloc_always_2,
    __wine_asan_delayloader___asan_stack_malloc_always_3,
    __wine_asan_delayloader___asan_stack_malloc_always_4,
    __wine_asan_delayloader___asan_stack_malloc_always_5,
    __wine_asan_delayloader___asan_stack_malloc_always_6,
    __wine_asan_delayloader___asan_stack_malloc_always_7,
    __wine_asan_delayloader___asan_stack_malloc_always_8,
    __wine_asan_delayloader___asan_stack_malloc_always_9,
    __wine_asan_delayloader___asan_stack_malloc_always_10,
    __wine_asan_delayloader___asan_load1,
    __wine_asan_delayloader___asan_load2,
    __wine_asan_delayloader___asan_load4,
    __wine_asan_delayloader___asan_load8,
    __wine_asan_delayloader___asan_load16,
    __wine_asan_delayloader___asan_store1,
    __wine_asan_delayloader___asan_store2,
    __wine_asan_delayloader___asan_store4,
    __wine_asan_delayloader___asan_store8,
    __wine_asan_delayloader___asan_store16,
    __wine_asan_delayloader___asan_loadN,
    __wine_asan_delayloader___asan_storeN,
    __wine_asan_delayloader___asan_load1_noabort,
    __wine_asan_delayloader___asan_load2_noabort,
    __wine_asan_delayloader___asan_load4_noabort,
    __wine_asan_delayloader___asan_load8_noabort,
    __wine_asan_delayloader___asan_load16_noabort,
    __wine_asan_delayloader___asan_store1_noabort,
    __wine_asan_delayloader___asan_store2_noabort,
    __wine_asan_delayloader___asan_store4_noabort,
    __wine_asan_delayloader___asan_store8_noabort,
    __wine_asan_delayloader___asan_store16_noabort,
    __wine_asan_delayloader___asan_loadN_noabort,
    __wine_asan_delayloader___asan_storeN_noabort,
    __wine_asan_delayloader___asan_report_load1,
    __wine_asan_delayloader___asan_report_load2,
    __wine_asan_delayloader___asan_report_load4,
    __wine_asan_delayloader___asan_report_load8,
    __wine_asan_delayloader___asan_report_load16,
    __wine_asan_delayloader___asan_report_store1,
    __wine_asan_delayloader___asan_report_store2,
    __wine_asan_delayloader___asan_report_store4,
    __wine_asan_delayloader___asan_report_store8,
    __wine_asan_delayloader___asan_report_store16,
    __wine_asan_delayloader___asan_report_load_n,
    __wine_asan_delayloader___asan_report_store_n,
    __wine_asan_delayloader___asan_report_load1_noabort,
    __wine_asan_delayloader___asan_report_load2_noabort,
    __wine_asan_delayloader___asan_report_load4_noabort,
    __wine_asan_delayloader___asan_report_load8_noabort,
    __wine_asan_delayloader___asan_report_load16_noabort,
    __wine_asan_delayloader___asan_report_store1_noabort,
    __wine_asan_delayloader___asan_report_store2_noabort,
    __wine_asan_delayloader___asan_report_store4_noabort,
    __wine_asan_delayloader___asan_report_store8_noabort,
    __wine_asan_delayloader___asan_report_store16_noabort,
    __wine_asan_delayloader___asan_report_load_n_noabort,
    __wine_asan_delayloader___asan_report_store_n_noabort,
    __wine_asan_delayloader___asan_exp_load1,
    __wine_asan_delayloader___asan_exp_load2,
    __wine_asan_delayloader___asan_exp_load4,
    __wine_asan_delayloader___asan_exp_load8,
    __wine_asan_delayloader___asan_exp_load16,
    __wine_asan_delayloader___asan_exp_store1,
    __wine_asan_delayloader___asan_exp_store2,
    __wine_asan_delayloader___asan_exp_store4,
    __wine_asan_delayloader___asan_exp_store8,
    __wine_asan_delayloader___asan_exp_store16,
    __wine_asan_delayloader___asan_exp_loadN,
    __wine_asan_delayloader___asan_exp_storeN,
    __wine_asan_delayloader___asan_memcpy,
    __wine_asan_delayloader___asan_memset,
    __wine_asan_delayloader___asan_memmove,
    __wine_asan_delayloader___asan_poison_cxx_array_cookie,
    __wine_asan_delayloader___asan_load_cxx_array_cookie,
    __wine_asan_delayloader___asan_poison_intra_object_redzone,
    __wine_asan_delayloader___asan_unpoison_intra_object_redzone,
    __wine_asan_delayloader___asan_alloca_poison,
    __wine_asan_delayloader___asan_allocas_unpoison,
    __wine_asan_delayloader___asan_default_suppressions,
    __wine_asan_delayloader___asan_handle_vfork
};
/* clang-format on */
