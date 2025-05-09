/* Instead of forwarding calls from PE to unix, it's more efficient to just duplicate these
 * shadow memory accessing functions on both sides, since the shadow memory address would
 * be the same anyways. */
#define _RET_IP_ __builtin_return_address(0)

#define ASAN_SHADOW_TO_MEM(shadow)                                                                 \
    ((ULONG_PTR)(shadow - ASAN_SHADOW_OFFSET) << ASAN_SHADOW_SCALE_SHIFT)

static BOOL NOBUILTIN asan_report(const void *addr, SIZE_T size, BOOL is_write, void *ip)
{
    UINT8 *shadow = (UINT8 *)ASAN_MEM_TO_SHADOW(addr), *stack_start, *stack_end;
    INT8 *probe = (INT8 *)shadow;
    USHORT frames, i;
    void *stack[32];
    ERR("ASan: %s of %llu bytes at %p, caller %p\n", is_write ? "write" : "read",
            (unsigned long long)size, addr, ip);
#ifndef WINE_UNIX_LIB
    frames = RtlCaptureStackBackTrace(2, 32, stack, NULL);
    if (frames)
    {
        ERR("stacktrace:\n");
        for (i = 0; i < frames; i++) ERR("\t%p\n", stack[i]);
    }
#else
    (void)frames;
    (void)i;
    (void)stack;
#endif
    ERR("info:\n");
    if (!*shadow)
    {
        SIZE_T offset = 0;
        if (size <= 8) return TRUE; /* huh?? */
        while (!*shadow)
        {
            offset += ASAN_GRANULE_SIZE;
            shadow++;
        }
        if (offset >= size)
        {
            ERR("\tno poisoned bytes\n");
            return TRUE; /* huh??? */
        }
        ERR("\tfirst poisoned byte is %p+%llu = %p\n", addr, (unsigned long long)offset,
                (void *)((ULONG_PTR)addr + offset));
        probe = (INT8 *)shadow;
        addr = (const INT8 *)addr + offset;
    }
    if (*shadow < 0x80)
    {
        ERR("\tpartial granule: %u\n", *shadow);
        shadow++;
        probe++;
    }
    switch (*shadow)
    {
    case ASAN_STACK_LEFT_REDZONE_MAGIC:
        while (*probe < 0) probe++;
        ERR("\tstack-buffer-underflow, addr is %lu bytes to the left of the start of stack\n",
                (unsigned long)(ASAN_SHADOW_TO_MEM(probe) - (ULONG_PTR)addr));
        break;
    case ASAN_STACK_MID_REDZONE_MAGIC:
    case ASAN_STACK_RIGHT_REDZONE_MAGIC:
        while (*probe < 0) probe--;
        ERR("\tstack-buffer-overflow, addr is %lu bytes to the right of %s\n",
                (unsigned long)((ULONG_PTR)addr - ASAN_SHADOW_TO_MEM(probe)),
                *shadow == ASAN_STACK_MID_REDZONE_MAGIC ? "another stack variable" : "the end of stack");
        break;
    default: ERR("\tshadow: %02x\n", *shadow); break;
    }

    switch (*shadow)
    {
    case ASAN_STACK_LEFT_REDZONE_MAGIC:
    case ASAN_STACK_MID_REDZONE_MAGIC:
    case ASAN_STACK_RIGHT_REDZONE_MAGIC:
        stack_start = stack_end = (UINT8 *)probe;
        while (*stack_start != ASAN_STACK_LEFT_REDZONE_MAGIC) stack_start--;
        while (*stack_end != ASAN_STACK_RIGHT_REDZONE_MAGIC) stack_end++;
        stack_start++;
        ERR("\tstack: [%p, %p)\n", (void *)ASAN_SHADOW_TO_MEM(stack_start), (void *)ASAN_SHADOW_TO_MEM(stack_end));
        break;
    default: break;
    }
    return TRUE;
}

static FORCEINLINE BOOL memory_is_poisoned_1(const void *addr)
{
    INT8 shadow_value = *(INT8 *)ASAN_MEM_TO_SHADOW(addr);

    if (shadow_value)
    {
        INT8 last_accessible_byte = (ULONG_PTR)addr & ASAN_GRANULE_MASK;
        return last_accessible_byte >= shadow_value;
    }

    return FALSE;
}

static FORCEINLINE BOOL memory_is_poisoned_2_4_8(const void *addr, SIZE_T size)
{
    UINT8 *shadow_addr = (UINT8 *)ASAN_MEM_TO_SHADOW(addr);

    /*
     * Access crosses 8(shadow size)-byte boundary. Such access maps
     * into 2 shadow bytes, so we need to check them both.
     */
    if ((((ULONG_PTR)addr + size - 1) & ASAN_GRANULE_MASK) < size - 1)
        return *shadow_addr || memory_is_poisoned_1((const INT8 *)addr + size - 1);

    return memory_is_poisoned_1((const INT8 *)addr + size - 1);
}

static FORCEINLINE BOOL memory_is_poisoned_16(const void *addr)
{
    UINT16 *shadow_addr = (UINT16 *)ASAN_MEM_TO_SHADOW(addr);

    /* Unaligned 16-bytes access maps into 3 shadow bytes. */
    if (((ULONG_PTR)addr) & ASAN_GRANULE_MASK)
        return *shadow_addr || memory_is_poisoned_1((const INT8 *)addr + 15);

    return *shadow_addr;
}

static FORCEINLINE ULONG_PTR bytes_is_nonzero(const UINT8 *start, SIZE_T size)
{
    while (size)
    {
        if (*start) return (ULONG_PTR)start;
        start++;
        size--;
    }

    return 0;
}

static FORCEINLINE ULONG_PTR memory_is_nonzero(const UINT8 *start, const UINT8 *end)
{
    unsigned int words;
    ULONG_PTR ret;
    unsigned int prefix = (unsigned int)((ULONG_PTR)start % 8);

    if (end - start <= 16) return bytes_is_nonzero(start, end - start);

    if (prefix)
    {
        prefix = 8 - prefix;
        ret = bytes_is_nonzero(start, prefix);
        if (ret) return ret;
        start += prefix;
    }

    words = (unsigned int)((end - start) / 8);
    while (words)
    {
        if (*(UINT64 *)start) return bytes_is_nonzero(start, 8);
        start += 8;
        words--;
    }

    return bytes_is_nonzero(start, (end - start) % 8);
}

static FORCEINLINE BOOL memory_is_poisoned_n(const void *addr, SIZE_T size)
{
    ULONG_PTR ret;

    ret = memory_is_nonzero((const UINT8 *)ASAN_MEM_TO_SHADOW(addr),
            (const UINT8 *)ASAN_MEM_TO_SHADOW((const UINT8 *)addr + size - 1) + 1);

    if (ret)
    {
        const INT8 *last_byte = (const INT8 *)addr + size - 1;
        INT8 *last_shadow = (INT8 *)ASAN_MEM_TO_SHADOW(last_byte);
        INT8 last_accessible_byte = (ULONG_PTR)last_byte & ASAN_GRANULE_MASK;

        if (ret != (ULONG_PTR)last_shadow || last_accessible_byte >= *last_shadow) return TRUE;
    }
    return FALSE;
}

static FORCEINLINE BOOL memory_is_poisoned(const void *addr, SIZE_T size)
{
    if (__builtin_constant_p(size))
    {
        switch (size)
        {
        case 1: return memory_is_poisoned_1(addr);
        case 2:
        case 4:
        case 8: return memory_is_poisoned_2_4_8(addr, size);
        case 16: return memory_is_poisoned_16(addr);
        default: asan_abort();
        }
    }

    return memory_is_poisoned_n(addr, size);
}

static FORCEINLINE BOOL check_region_inline(const char *addr, SIZE_T size, BOOL write, void *ret_ip)
{
    if (size == 0) return TRUE;

    if ((ULONG_PTR)addr + size < (ULONG_PTR)addr) return !asan_report(addr, size, write, ret_ip);

    if (!memory_is_poisoned(addr, size)) return TRUE;

    return !asan_report(addr, size, write, ret_ip);
}

static FORCEINLINE void check_region_inline_abort(const char *addr, SIZE_T size, BOOL write, void *ret_ip)
{
    if (!check_region_inline(addr, size, write, ret_ip)) asan_abort();
}

#define DEFINE_ASAN_LOAD_STORE(size)                                                               \
    ASANAPI void __asan_load##size(void *addr)                                                     \
    {                                                                                              \
        check_region_inline_abort(addr, size, FALSE, _RET_IP_);                                    \
    }                                                                                              \
    ASANAPI void __asan_store##size(void *addr)                                                    \
    {                                                                                              \
        check_region_inline_abort(addr, size, TRUE, _RET_IP_);                                     \
    }

DEFINE_ASAN_LOAD_STORE(1)
DEFINE_ASAN_LOAD_STORE(2)
DEFINE_ASAN_LOAD_STORE(4)
DEFINE_ASAN_LOAD_STORE(8)
DEFINE_ASAN_LOAD_STORE(16)

ASANAPI void __asan_loadN(void *addr, SIZE_T size)
{
    check_region_inline_abort(addr, size, FALSE, _RET_IP_);
}

ASANAPI void __asan_storeN(void *addr, SIZE_T size)
{
    check_region_inline_abort(addr, size, TRUE, _RET_IP_);
}

#define DEFINE_ASAN_LOAD_STORE_NOABORT(size)                                                       \
    ASANAPI void __asan_load##size##_noabort(void *addr)                                           \
    {                                                                                              \
        check_region_inline(addr, size, FALSE, _RET_IP_);                                          \
    }                                                                                              \
    ASANAPI void __asan_store##size##_noabort(void *addr)                                          \
    {                                                                                              \
        check_region_inline(addr, size, TRUE, _RET_IP_);                                           \
    }

DEFINE_ASAN_LOAD_STORE_NOABORT(1)
DEFINE_ASAN_LOAD_STORE_NOABORT(2)
DEFINE_ASAN_LOAD_STORE_NOABORT(4)
DEFINE_ASAN_LOAD_STORE_NOABORT(8)
DEFINE_ASAN_LOAD_STORE_NOABORT(16)

ASANAPI void __asan_loadN_noabort(void *addr, SIZE_T size)
{
    check_region_inline(addr, size, FALSE, _RET_IP_);
}

ASANAPI void __asan_storeN_noabort(void *addr, SIZE_T size)
{
    check_region_inline(addr, size, TRUE, _RET_IP_);
}

/* Emitted by the compiler to [un]poison local variables. */
#define DEFINE_ASAN_SET_SHADOW(byte)                                                               \
    ASANAPI void __asan_set_shadow_##byte(const void *addr, SIZE_T size)                           \
    {                                                                                              \
        REAL(memset)((void *)addr, 0x##byte, size);                                                \
    }

DEFINE_ASAN_SET_SHADOW(00);
DEFINE_ASAN_SET_SHADOW(01);
DEFINE_ASAN_SET_SHADOW(02);
DEFINE_ASAN_SET_SHADOW(03);
DEFINE_ASAN_SET_SHADOW(04);
DEFINE_ASAN_SET_SHADOW(05);
DEFINE_ASAN_SET_SHADOW(06);
DEFINE_ASAN_SET_SHADOW(07);
DEFINE_ASAN_SET_SHADOW(f1);
DEFINE_ASAN_SET_SHADOW(f2);
DEFINE_ASAN_SET_SHADOW(f3);
DEFINE_ASAN_SET_SHADOW(f5);
DEFINE_ASAN_SET_SHADOW(f8);

/* Functions concerning memory load and store reporting */
#define DEFINE_ASAN_REPORT_LOAD_STORE(size)                                                        \
    ASANAPI void DECLSPEC_NORETURN __asan_report_load##size(void *addr)                            \
    {                                                                                              \
        asan_report(addr, size, FALSE, _RET_IP_);                                                  \
        asan_abort();                                                                              \
    }                                                                                              \
    ASANAPI void DECLSPEC_NORETURN __asan_report_store##size(void *addr)                           \
    {                                                                                              \
        asan_report(addr, size, TRUE, _RET_IP_);                                                   \
        asan_abort();                                                                              \
    }                                                                                              \
    ASANAPI void __asan_report_load##size##_noabort(void *addr)                                    \
    {                                                                                              \
        asan_report(addr, size, FALSE, _RET_IP_);                                                  \
    }                                                                                              \
    ASANAPI void __asan_report_store##size##_noabort(void *addr)                                   \
    {                                                                                              \
        asan_report(addr, size, TRUE, _RET_IP_);                                                   \
    }

DEFINE_ASAN_REPORT_LOAD_STORE(1)
DEFINE_ASAN_REPORT_LOAD_STORE(2)
DEFINE_ASAN_REPORT_LOAD_STORE(4)
DEFINE_ASAN_REPORT_LOAD_STORE(8)
DEFINE_ASAN_REPORT_LOAD_STORE(16)

ASANAPI void DECLSPEC_NORETURN __asan_report_load_n(void *addr, SIZE_T size)
{
    asan_report(addr, size, FALSE, _RET_IP_);
    asan_abort();
}
ASANAPI void DECLSPEC_NORETURN __asan_report_store_n(void *addr, SIZE_T size)
{
    asan_report(addr, size, TRUE, _RET_IP_);
    asan_abort();
}
ASANAPI void __asan_report_load_n_noabort(void *addr, SIZE_T size)
{
    asan_report(addr, size, FALSE, _RET_IP_);
}
ASANAPI void __asan_report_store_n_noabort(void *addr, SIZE_T size)
{
    asan_report(addr, size, TRUE, _RET_IP_);
}

/* Functions concerning memory load and store (experimental variants) */
#define DEFINE_ASAN_EXP_LOAD_STORE(size)                                                           \
    ASANAPI void __asan_exp_load##size(void *addr, UINT32 exp)                                     \
    {                                                                                              \
        __asan_load##size(addr);                                                                   \
    }                                                                                              \
    ASANAPI void __asan_exp_store##size(void *addr, UINT32 exp)                                    \
    {                                                                                              \
        __asan_store##size(addr);                                                                  \
    }

DEFINE_ASAN_EXP_LOAD_STORE(1)
DEFINE_ASAN_EXP_LOAD_STORE(2)
DEFINE_ASAN_EXP_LOAD_STORE(4)
DEFINE_ASAN_EXP_LOAD_STORE(8)
DEFINE_ASAN_EXP_LOAD_STORE(16)

void __asan_exp_loadN(void *addr, SIZE_T size, UINT32 exp)
{
    __asan_loadN(addr, size);
}
void __asan_exp_storeN(void *addr, SIZE_T size, UINT32 exp)
{
    __asan_storeN(addr, size);
}

/* Functions concerning memory load and store reporting (experimental variants) */
#define DEFINE_ASAN_REPORT_EXP_LOAD_STORE(size)                                                    \
    ASANAPI void DECLSPEC_NORETURN __asan_report_exp_load##size(void *addr, UINT32 exp)            \
    {                                                                                              \
        asan_abort();                                                                              \
    }                                                                                              \
    ASANAPI void DECLSPEC_NORETURN __asan_report_exp_store##size(void *addr, UINT32 exp)           \
    {                                                                                              \
        asan_abort();                                                                              \
    }

DEFINE_ASAN_REPORT_EXP_LOAD_STORE(1)
DEFINE_ASAN_REPORT_EXP_LOAD_STORE(2)
DEFINE_ASAN_REPORT_EXP_LOAD_STORE(4)
DEFINE_ASAN_REPORT_EXP_LOAD_STORE(8)
DEFINE_ASAN_REPORT_EXP_LOAD_STORE(16)

ASANAPI void DECLSPEC_NORETURN __asan_report_exp_load_n(void *addr, SIZE_T size, UINT32 exp)
{
    asan_abort();
}
ASANAPI void DECLSPEC_NORETURN __asan_report_exp_store_n(void *addr, SIZE_T size, UINT32 exp)
{
    asan_abort();
}

/* Check if `addr` has a corresponding shadow address. */
static inline BOOL addr_in_mem(ULONG_PTR addr, SIZE_T size)
{
    ULONG_PTR shadow = ASAN_MEM_TO_SHADOW(addr);
    SIZE_T shadow_size = ALIGNUP(size, ASAN_GRANULE_SIZE) / ASAN_GRANULE_SIZE;
    if (shadow >= ASAN_LOW_SHADOW_START && shadow <= ASAN_LOW_SHADOW_END - shadow_size + 1)
        return TRUE;
    if (shadow >= ASAN_HIGH_SHADOW_START && shadow <= ASAN_HIGH_SHADOW_END - shadow_size + 1)
        return TRUE;
    return FALSE;
}

/* Functions concerning the poisoning of memory */

/* Mark [addr, addr + size) as poisoned. Assuming addr is aligned to
 * ASAN_GRANULE_SIZE. */
void __wine_asan_poison_aligned_memory(void const volatile *addr, SIZE_T size, BYTE poison)
{
    INT8 end_offset = (INT8)(size & ASAN_GRANULE_MASK);
    INT8 *shadow_end = (INT8 *)ASAN_MEM_TO_SHADOW(addr) + (size >> ASAN_SHADOW_SCALE_SHIFT);
    INT8 end_value;
    REAL(memset)((void *)ASAN_MEM_TO_SHADOW(addr), poison, size >> ASAN_SHADOW_SCALE_SHIFT);
    if (!end_offset) return;
    end_value = *shadow_end;
    if (poison)
    {
        if (end_value > 0 && end_value <= end_offset) *shadow_end = poison;
    }
    else
    {
        if (end_value != 0 && end_value < end_offset) *shadow_end = end_offset;
    }
}

/* Mark [addr, addr + size) as poisoned. Either or both ends may be unaligned. */
static inline void asan_poison_memory(void const volatile *addr, SIZE_T size, BYTE poison)
{
    INT8 start_offset = (INT8)((ULONG_PTR)addr & ASAN_GRANULE_MASK);
    INT8 *shadow = (INT8 *)ASAN_MEM_TO_SHADOW(addr);

    if (start_offset == 0)
    {
        __wine_asan_poison_aligned_memory(addr, size, poison);
        return;
    }
    if (size < ASAN_GRANULE_SIZE - start_offset)
    {
        /* [addr, addr + size) is contained within a single shadow byte */
        if (poison)
        {
            if (*shadow > 0 && *shadow <= start_offset + size && start_offset < *shadow)
                *shadow = start_offset;
        }
        else
        {
            if (*shadow > 0 && *shadow < start_offset + size) *shadow = start_offset + size;
        }
        return;
    }
    addr = (void const volatile *)(((ULONG_PTR)addr & ~ASAN_GRANULE_MASK) + ASAN_GRANULE_SIZE);
    size = size - (ASAN_GRANULE_SIZE - start_offset);
    __wine_asan_poison_aligned_memory(addr, size, poison);
    if (poison)
    {
        if (*shadow == 0 || *shadow > start_offset) *shadow = start_offset;
    }
    else if (*shadow >= start_offset) *shadow = 0;
}

ASANAPI void __asan_poison_memory_region(void const volatile *addr, SIZE_T size)
{
    if (size == 0) return;
    asan_poison_memory(addr, size, ASAN_USER_POISONED_MEMORY_MAGIC);
}

ASANAPI void __asan_unpoison_memory_region(void const volatile *addr, SIZE_T size)
{
    if (size == 0) return;
    asan_poison_memory(addr, size, 0);
}

/* For 64-bit PE, 32/64bit Unix, the stack is aligned to 16 bytes.
 * For 32-bit PE, the stack is aligned to 4 bytes. */
#if defined(_WIN64) || defined(WINE_UNIX_LIB)
ASANAPI void __asan_poison_stack_memory(void const volatile *addr, SIZE_T size)
{
    if (size == 0) return;
    __wine_asan_poison_aligned_memory(addr, size, ASAN_STACK_USE_AFTER_SCOPE_MAGIC);
}

ASANAPI void __asan_unpoison_stack_memory(void const volatile *addr, SIZE_T size)
{
    if (size == 0) return;
    __wine_asan_poison_aligned_memory(addr, size, 0);
}
#else
ASANAPI void __asan_poison_stack_memory(void const volatile *addr, SIZE_T size)
{
    if (size == 0) return;
    asan_poison_memory(addr, size, ASAN_STACK_USE_AFTER_SCOPE_MAGIC);
}
ASANAPI void __asan_unpoison_stack_memory(void const volatile *addr, SIZE_T size)
{
    if (size == 0) return;
    asan_poison_memory(addr, size, 0);
}
#endif

/* Functions concerning query about whether memory is poisoned */
int __asan_address_is_poisoned(void const volatile *addr)
{
    if (!addr_in_mem((ULONG_PTR)addr, 1))
    {
        ERR("addr %p not in shadow\n", addr);
        return 1;
    }
    return memory_is_poisoned_1((const void *)addr);
}
void *__asan_region_is_poisoned(void *beg, SIZE_T size)
{
    ULONG_PTR start = (ULONG_PTR)beg;
    ULONG_PTR ptr = (start + ASAN_GRANULE_SIZE - 1) & ~ASAN_GRANULE_MASK;
    INT8 *shadow = (INT8 *)ASAN_MEM_TO_SHADOW(ptr);
    if (size == 0) return 0;
    if (start + size < start) return (void *)~((ULONG_PTR)0);
    if (!addr_in_mem(start, size)) return beg;
    if (start != ptr)
    {
        ULONG_PTR start_aligned = ptr - ASAN_GRANULE_SIZE;
        INT8 shadow_value = *(INT8 *)ASAN_MEM_TO_SHADOW(beg);
        if (shadow_value)
        {
            if (shadow_value <= start - start_aligned) return beg;
            /* If [beg, beg+size) are all in the same granule */
            if (start + size < ptr && shadow_value >= start + size - start_aligned) return 0;
            return (void *)(start_aligned + shadow_value);
        }
    }
    while (ptr < start + size)
    {
        INT8 shadow_value = *shadow;
        if (!shadow_value)
        {
            /* 8 bytes ok */
            ptr += ASAN_GRANULE_SIZE;
            shadow++;
            continue;
        }
        if (shadow_value < 0)
        {
            /* no bytes ok */
            return (void *)ptr;
        }
        if (shadow_value > start + size - ptr)
        {
            /* all bytes left to check are ok */
            return 0;
        }
        return (void *)(ptr + shadow_value);
    }
    return 0;
}

UINT32 __wine_asan_max_quarantine_size(void)
{
    struct asan_thread_state *state = asan_get_thread_state();
    return state->max_quarantine_size;
}
