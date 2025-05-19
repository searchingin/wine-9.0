/* Common code for managing ASan fake stacks, shared between Unix & PE. Modelled mostly after
 * upstream libasan */

#define ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT 6
#define ASAN_FAKE_STACK_MAX_FRAME_SIZE_SHIFT 16
#define ASAN_FAKE_STACK_MIN_SIZE_SHIFT 16
#ifdef _WIN64
#define ASAN_FAKE_STACK_MAX_SIZE_SHIFT 28
#define ASAN_FAKE_STACK_MAX_SIZE_SHIFT_32BIT 24
#else
#define ASAN_FAKE_STACK_MAX_SIZE_SHIFT 24
#endif
#define ASAN_FAKE_STACK_NUM_OF_SIZE_CLASSES                                                        \
    (ASAN_FAKE_STACK_MAX_FRAME_SIZE_SHIFT - ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT + 1)

/* Fake stacks are divided into 11 size classes, starting from 64B, each of which has the same
 * size as `stack_size == 1 << stack_shift`, plus an array of flags to track which frame is allocated.
 *
 * Flag bytes needed for each size class:
 *      64B: stack_size >>  6,
 *     128B: stack_size >>  7,
 *      ...:              ...,
 *     64KB: stack_size >> 16,
 *
 * In total: stack_size >> 16 * 2047, approximately stack_size >> 5
 *
 * Plus another page for some metadata.
 */
struct asan_fake_stack
{
    SIZE_T page_size;
    SIZE_T stack_shift;
    SIZE_T allocation_head[ASAN_FAKE_STACK_NUM_OF_SIZE_CLASSES];
    ULONG flags;
};

#define ASAN_FAKE_STACK_FLAGS_EXHAUSTED_BIT ((ULONG)1)

struct asan_fake_frame
{
    ULONG_PTR magic; // Modified by the instrumented code.
    ULONG_PTR descr; // Modified by the instrumented code.
    ULONG_PTR pc;    // Modified by the instrumented code.
    ULONG_PTR real_stack;
};

static FORCEINLINE UINT8 *asan_fake_stack_get_flags(struct asan_fake_stack *fs, SIZE_T size_class)
{
    UINT8 *flags = (UINT8 *)fs + fs->page_size;
    SIZE_T offset_shift = fs->stack_shift - ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT + 1;
    return flags + (1 << offset_shift) - (1 << (offset_shift - size_class));
}

static FORCEINLINE SIZE_T asan_fake_stack_flags_size(SIZE_T stack_shift)
{
    return 1 << (stack_shift - ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT + 1);
}

static FORCEINLINE SIZE_T asan_fake_stack_total_size(SIZE_T stack_shift, SIZE_T page_size)
{
    SIZE_T size = ASAN_FAKE_STACK_NUM_OF_SIZE_CLASSES * (1 << stack_shift) + asan_fake_stack_flags_size(stack_shift);
    size = ALIGNUP(size, page_size) + page_size;
    return size;
}

static FORCEINLINE struct asan_fake_frame *asan_fake_stack_get_frame(struct asan_fake_stack *fs,
        SIZE_T size_class, SIZE_T pos)
{
    /*
     * 1 page of metadata + arrays of flags + bytes_per_size_class * size_class +
     * bytes_per_frame(size_class) * pos
     */
    SIZE_T offset = fs->page_size + (1 << (fs->stack_shift - ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT + 1)) +
                    (1 << fs->stack_shift) * size_class +
                    (1 << (ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT + size_class)) * pos;
    return (struct asan_fake_frame *)((UINT8 *)fs + offset);
}

static FORCEINLINE UINT8 **asan_fake_frame_saved_flag_ptr(struct asan_fake_frame *ff, SIZE_T size_class)
{
    return (UINT8 **)((UINT8 *)ff + (1 << (ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT + size_class))) - 1;
}

static FORCEINLINE void *asan_fake_stack_allocate(struct asan_fake_stack *fs, SIZE_T size_class, ULONG_PTR rsp)
{
    UINT8 *flags = asan_fake_stack_get_flags(fs, size_class);
    SIZE_T i, num_of_frames = 1 << (fs->stack_shift - ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT - size_class);
    struct asan_fake_frame *ret;
    for (i = 0; i < num_of_frames; i++)
    {
        SIZE_T pos = fs->allocation_head[size_class]++;
        fs->allocation_head[size_class] %= num_of_frames;

        if (flags[pos]) continue;
        flags[pos] = 1;

        ret = asan_fake_stack_get_frame(fs, size_class, pos);
        ret->real_stack = rsp;
        *asan_fake_frame_saved_flag_ptr(ret, size_class) = &flags[pos];
        return ret;
    }
    /* Ran out of available frames */
    if (!(fs->flags & ASAN_FAKE_STACK_FLAGS_EXHAUSTED_BIT))
    {
        WARN("fake stack exhausted, size_class %u\n", (unsigned)size_class);
        fs->flags |= ASAN_FAKE_STACK_FLAGS_EXHAUSTED_BIT;
    }
    return NULL;
}

static void asan_fake_stack_gc(struct asan_fake_stack *fs, ULONG_PTR rsp)
{
    /* TODO: make this faster. GC basically happens for every user callback, scanning all fake
     * frames can be slow since there are many of them. maybe keep a cache of recently allocated
     * frames. */
#ifdef WINE_UNIX_LIB
    ULONG_PTR stack_top = (ULONG_PTR)ntdll_get_thread_data()->kernel_stack, stack_bottom = stack_top + kernel_stack_size;
#else
    ULONG_PTR stack_top = (ULONG_PTR)NtCurrentTeb()->Tib.StackLimit,
              stack_bottom = (ULONG_PTR)NtCurrentTeb()->Tib.StackBase;
#endif
    int size_class, count = 0;
    DWORD freed = 0;
    if (rsp <= stack_top || rsp > stack_bottom) return;
    for (size_class = 0; size_class < ASAN_FAKE_STACK_NUM_OF_SIZE_CLASSES; size_class++)
    {
        UINT8 *flags = asan_fake_stack_get_flags(fs, size_class);
        SIZE_T i, num_of_frames = 1 << (fs->stack_shift - size_class - ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT),
                  size_class_size = 1 << (size_class + ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT);
        for (i = 0; i < num_of_frames; i++)
        {
            struct asan_fake_frame *ff;
            if (!flags[i]) continue;
            ff = asan_fake_stack_get_frame(fs, size_class, i);
            if (ff->real_stack > stack_top && ff->real_stack < rsp)
            {
                flags[i] = 0;
                count++;
                freed += size_class_size;
                REAL(memset)((void *)ASAN_MEM_TO_SHADOW(ff), ASAN_STACK_AFTER_RETURN_MAGIC,
                        size_class_size / ASAN_GRANULE_SIZE);
            }
        }
    }

    if (count)
    {
        TRACE("freed %d fake stack frames, %lu bytes\n", count, (unsigned long)freed);
        fs->flags &= ~ASAN_FAKE_STACK_FLAGS_EXHAUSTED_BIT;
    }
}

/* In wine, `on_malloc` is used for more than just allocating a fake stack frame. Unpoisoning the
 * stack in __asan_handle_no_return is a big problem for us, since wine can have 2 to 3 different
 * active stacks per thread. A noreturn function might unwind either or both of them, so we have to
 * blindly unpoison all stacks, but it's difficult to know the current top and boundaries of every
 * stack all at once. So the unpoisoning is deferred until the prologue of the next
 * native/WoW64/unix function. Since `on_malloc` is called from every prologues, it essentially
 * functions as a hook. */
static FORCEINLINE void *asan_fake_stack_on_malloc(SIZE_T size_class, SIZE_T size)
{
    struct asan_fake_stack *fs = asan_get_fake_stack();
    void *ff;
    ULONG_PTR *saved_sp_ptr = asan_get_sp();
    ULONG_PTR sp = (ULONG_PTR)__builtin_frame_address(0), saved_sp = saved_sp_ptr ? *saved_sp_ptr : 0;

    if (saved_sp_ptr) *saved_sp_ptr = sp;
    if (asan_state_check_and_reset_unpoison_stack() && saved_sp)
        __asan_unpoison_stack_memory((void *)saved_sp, asan_get_stack_base() - saved_sp);

    if (!fs) return NULL; /* Fake stack not yet created, or we are current on sigaltstack. */

    if (asan_state_check_and_reset_gc()) asan_fake_stack_gc(fs, sp);

    ff = asan_fake_stack_allocate(fs, size_class, sp);
    if (!ff) return NULL;

    size = ALIGNUP(size, ASAN_GRANULE_SIZE);
    REAL(memset)((void *)ASAN_MEM_TO_SHADOW(ff), 0, size / ASAN_GRANULE_SIZE);
    return (void *)ff;
}

#define DEFINE_ASAN_STACK_MALLOC_FREE(size_class)                                                                       \
    ASANAPI void *__asan_stack_malloc_##size_class(SIZE_T size)                                                         \
    {                                                                                                                   \
        return asan_fake_stack_on_malloc(size_class, size);                                                             \
    }                                                                                                                   \
    ASANAPI void *__asan_stack_malloc_always_##size_class(SIZE_T size)                                                  \
    {                                                                                                                   \
        return asan_fake_stack_on_malloc(size_class, size);                                                             \
    }                                                                                                                   \
    ASANAPI void __asan_stack_free_##size_class(void *ptr, SIZE_T size)                                                 \
    {                                                                                                                   \
        **asan_fake_frame_saved_flag_ptr((struct asan_fake_frame *)ptr, size_class) = 0;                                \
        size = ALIGNUP(size, ASAN_GRANULE_SIZE);                                                                        \
        REAL(memset)((void *)ASAN_MEM_TO_SHADOW((void *)ptr), ASAN_STACK_AFTER_RETURN_MAGIC, size / ASAN_GRANULE_SIZE); \
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
    struct asan_fake_stack *fs = asan_get_fake_stack();
    struct asan_fake_frame *ff;
    SIZE_T total_size, size_class, pos, size_class_size;
    UINT8 *flags;
    if (!fs) return FALSE;

    /* Is `addr` inside our fake stack at all? */
    total_size = asan_fake_stack_total_size(fs->stack_shift, fs->page_size);
    if (addr <= (ULONG_PTR)fs || addr >= (ULONG_PTR)fs + total_size) return FALSE;

    /* Calculate addr as offset into the frames region */
    addr -= (ULONG_PTR)fs + fs->page_size + asan_fake_stack_flags_size(fs->stack_shift);
    size_class = addr / (1 << fs->stack_shift);
    addr %= 1 << fs->stack_shift;
    size_class_size = 1 << (ASAN_FAKE_STACK_MIN_FRAME_SIZE_SHIFT + size_class);
    pos = addr / size_class_size;
    addr %= size_class_size; /* offset into the fake frame */
    flags = asan_fake_stack_get_flags(fs, size_class);
    ff = asan_fake_stack_get_frame(fs, size_class, pos);
    if (!flags[pos])
    {
        WARN("retired fake frame\n");
        return FALSE; /* Frame is not active, use-after-return? */
    }

    if (beg) *beg = (ULONG_PTR)ff;
    if (end) *end = (ULONG_PTR)ff + size_class_size;
    if (real_stack) *real_stack = ff->real_stack + addr;
    return TRUE;
}
