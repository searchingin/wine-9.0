/* Common code for managing ASan fake stacks, shared between Unix & PE. Modelled mostly after
 * upstream libasan */

/* In AddressSanitizers, the fake stacks actually serve two purposes. First, some background on how
 * ASan works. ASan instrumented code will poison the stack in function prologues, it adds padding
 * around the stack frame and in between each stack variables and mark them as poisoned. So accesses
 * to these invalid memory locations can be detected. The stack is unpoisoned in function epilogues,
 * so everytime a function is called it can expect a clear, unpoisoned stack. However, some
 * functions might be `noreturn`, and does not go through the usual function epilogue. For example,
 * in C++, throwing an exception (`__cxa_throw`) is `noreturn`, and it unwinds the stack pointer to
 * some previous location. The ASan runtime needs to detect this and unpoisoned the unwound portion
 * of the stack, but it does not know how much of the stack is actually unwound. So without fake
 * stacks, it will unpoison the entire stack once a `noreturn` function is called, which means there
 * could be false negatives.
 *
 * Here's what fake stacks come in. Fake stacks are normally used to detect stack use-after-return,
 * by keeping expired stack frame around and detect accesses to them. To do this, a fake stack frame
 * is allocated in the function prologue. But this also means the ASan runtime gets a callback for
 * every function prologues. Combine that with the callback we already have before `noreturn`
 * functions are called, we can know the stack pointer location before and after every `noreturn`
 * function is called, and thus know which part of the stack to unpoison.
 */

#define DEFINE_ASAN_STACK_MALLOC_FREE(size_class)                                                  \
    ASANAPI void *__asan_stack_malloc_##size_class(SIZE_T size)                                    \
    {                                                                                              \
        return 0;                                                                                  \
    }                                                                                              \
    ASANAPI void *__asan_stack_malloc_always_##size_class(SIZE_T size)                             \
    {                                                                                              \
        return 0;                                                                                  \
    }                                                                                              \
    ASANAPI void __asan_stack_free_##size_class(void *ptr, SIZE_T size)                            \
    {                                                                                              \
    }

DEFINE_ASAN_STACK_MALLOC_FREE(0);
DEFINE_ASAN_STACK_MALLOC_FREE(1);
DEFINE_ASAN_STACK_MALLOC_FREE(2);
DEFINE_ASAN_STACK_MALLOC_FREE(3);
DEFINE_ASAN_STACK_MALLOC_FREE(4);
DEFINE_ASAN_STACK_MALLOC_FREE(5);
DEFINE_ASAN_STACK_MALLOC_FREE(6);
DEFINE_ASAN_STACK_MALLOC_FREE(7);
DEFINE_ASAN_STACK_MALLOC_FREE(8);
DEFINE_ASAN_STACK_MALLOC_FREE(9);
DEFINE_ASAN_STACK_MALLOC_FREE(10);

BOOL ASANAPI __wine_asan_is_inside_fake_stack_frame(ULONG_PTR addr, ULONG_PTR *beg, ULONG_PTR *end, ULONG_PTR *real_stack)
{
    return FALSE;
}
