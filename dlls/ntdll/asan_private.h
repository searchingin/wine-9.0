#ifdef WINE_UNIX_LIB
#define ASAN_FAKE_STACK_NEEDS_GC_BIT ((UINT8)1)
#else
#define ASAN_FAKE_STACK_NEEDS_GC_BIT ((UINT8)2)
#endif

#define ASAN_SHADOW_SCALE_SHIFT 3
#define ASAN_GRANULE_SIZE ((ULONG_PTR)1 << ASAN_SHADOW_SCALE_SHIFT)
#define ASAN_GRANULE_MASK (ASAN_GRANULE_SIZE - 1)
/* TODO: portability */
#ifdef _WIN64
#define ASAN_MAX_USERSPACE_ADDRESS ((ULONG_PTR)0x7fffffffffffUL)
#else
#define ASAN_MAX_USERSPACE_ADDRESS ((ULONG_PTR)0xffffffffUL)
#endif
#define ASAN_MEM_TO_SHADOW(addr)                                                                   \
    (((ULONG_PTR)(addr) >> ASAN_SHADOW_SCALE_SHIFT) + ASAN_SHADOW_OFFSET)
/* Shadow memory bounds, both ends inclusive */
#define ASAN_LOW_SHADOW_START ASAN_SHADOW_OFFSET
#define ASAN_LOW_SHADOW_END ASAN_MEM_TO_SHADOW(ASAN_SHADOW_OFFSET - 1)
#define ASAN_HIGH_SHADOW_START __wine_asan_high_shadow_start()
#define ASAN_HIGH_SHADOW_END ASAN_MEM_TO_SHADOW(ASAN_MAX_USERSPACE_ADDRESS)
#define ASAN_MIN_REDZONE_SIZE_SHIFT (4)
#define ASAN_MIN_REDZONE_SIZE                                                                      \
    (1 << ASAN_MIN_REDZONE_SIZE_SHIFT) /* must be greater than ASAN_GRANULE_SIZE */

#if WINE_ASAN
static inline __attribute__((always_inline)) ULONG_PTR __wine_asan_high_shadow_start(void)
{
    if (ASAN_HIGH_SHADOW_END >= ASAN_MAX_USERSPACE_ADDRESS - 0x1000) /* High shadow not needed */
        return ASAN_HIGH_SHADOW_END;
    return ASAN_MEM_TO_SHADOW(ASAN_HIGH_SHADOW_END + 1);
}

/* clang-format off */
enum asan_magics
{
    ASAN_STACK_LEFT_REDZONE_MAGIC    = 0xf1,
    ASAN_STACK_MID_REDZONE_MAGIC,
    ASAN_STACK_RIGHT_REDZONE_MAGIC,

    ASAN_STACK_AFTER_RETURN_MAGIC    = 0xf5,

    ASAN_USER_POISONED_MEMORY_MAGIC  = 0xf7,

    ASAN_STACK_USE_AFTER_SCOPE_MAGIC = 0xf8,

    ASAN_HEAP_REDZONE_MAGIC          = 0xfa,
    ASAN_HEAP_FREE_MAGIC             = 0xfd,
};
/* clang-format on */

struct asan_thread_state
{
    UINT8 unpoison_stack : 1;
    UINT8 unpoison_stack_unix : 1;
    UINT8 unpoison_stack_wow64 : 1;
};

/* Test if `addr` is inside a valid fake stack frame, also returns the fake stack frame bounds via
 * `beg` and `end`, and the corresponding real stack address for `addr`.
 * (If the out pointers are not NULL)
 *
 * Note `real_stack` might be slightly off, since the real stack frame pointer stored in the fake
 * frames' metadata will be above the correct value by a little bit, because of the frame pointer
 * and/or the return address stored on the stack. Since `real_stack` is mostly used to detect
 * whether a `EXCEPTION_REGISTRATION_RECORD` is on a particular frame or not, this should work as
 * long as the deviation is less than `sizeof(EXCEPTION_REGISTRATION_RECORD)`
 */
BOOL __wine_asan_is_inside_fake_stack_frame(ULONG_PTR addr, ULONG_PTR *beg, ULONG_PTR *end, ULONG_PTR *real_stack);
void wine_asan_init(void);
NTSTATUS wine_asan_alloc_thread(TEB *teb, SIZE_T user_stack_size);
#endif
