/* Instead of forwarding calls from PE to unix, it's more efficient to just duplicate these
 * shadow memory accessing functions on both sides, since the shadow memory address would
 * be the same anyways. */

#define DEFINE_ASAN_LOAD_STORE(size)                                                               \
    ASANAPI void __asan_load##size(void *addr)                                                     \
    {                                                                                              \
    }                                                                                              \
    ASANAPI void __asan_store##size(void *addr)                                                    \
    {                                                                                              \
    }

DEFINE_ASAN_LOAD_STORE(1)
DEFINE_ASAN_LOAD_STORE(2)
DEFINE_ASAN_LOAD_STORE(4)
DEFINE_ASAN_LOAD_STORE(8)
DEFINE_ASAN_LOAD_STORE(16)

ASANAPI void __asan_loadN(void *addr, SIZE_T size)
{
}

ASANAPI void __asan_storeN(void *addr, SIZE_T size)
{
}

#define DEFINE_ASAN_LOAD_STORE_NOABORT(size)                                                       \
    ASANAPI void __asan_load##size##_noabort(void *addr)                                           \
    {                                                                                              \
    }                                                                                              \
    ASANAPI void __asan_store##size##_noabort(void *addr)                                          \
    {                                                                                              \
    }

DEFINE_ASAN_LOAD_STORE_NOABORT(1)
DEFINE_ASAN_LOAD_STORE_NOABORT(2)
DEFINE_ASAN_LOAD_STORE_NOABORT(4)
DEFINE_ASAN_LOAD_STORE_NOABORT(8)
DEFINE_ASAN_LOAD_STORE_NOABORT(16)

ASANAPI void __asan_loadN_noabort(void *addr, SIZE_T size)
{
}

ASANAPI void __asan_storeN_noabort(void *addr, SIZE_T size)
{
}

/* Emitted by the compiler to [un]poison local variables. */
#define DEFINE_ASAN_SET_SHADOW(byte)                                                               \
    ASANAPI void __asan_set_shadow_##byte(const void *addr, SIZE_T size)                           \
    {                                                                                              \
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
        asan_abort();                                                                              \
    }                                                                                              \
    ASANAPI void DECLSPEC_NORETURN __asan_report_store##size(void *addr)                           \
    {                                                                                              \
        asan_abort();                                                                              \
    }                                                                                              \
    ASANAPI void __asan_report_load##size##_noabort(void *addr)                                    \
    {                                                                                              \
    }                                                                                              \
    ASANAPI void __asan_report_store##size##_noabort(void *addr)                                   \
    {                                                                                              \
    }

DEFINE_ASAN_REPORT_LOAD_STORE(1)
DEFINE_ASAN_REPORT_LOAD_STORE(2)
DEFINE_ASAN_REPORT_LOAD_STORE(4)
DEFINE_ASAN_REPORT_LOAD_STORE(8)
DEFINE_ASAN_REPORT_LOAD_STORE(16)

ASANAPI void DECLSPEC_NORETURN __asan_report_load_n(void *addr, SIZE_T size)
{
    asan_abort();
}
ASANAPI void DECLSPEC_NORETURN __asan_report_store_n(void *addr, SIZE_T size)
{
    asan_abort();
}
ASANAPI void __asan_report_load_n_noabort(void *addr, SIZE_T size)
{
}
ASANAPI void __asan_report_store_n_noabort(void *addr, SIZE_T size)
{
}

/* Functions concerning memory load and store (experimental variants) */
#define DEFINE_ASAN_EXP_LOAD_STORE(size)                                                           \
    ASANAPI void __asan_exp_load##size(void *addr, UINT32 exp)                                     \
    {                                                                                              \
        __asan_load##size((void *)addr);                                                           \
    }                                                                                              \
    ASANAPI void __asan_exp_store##size(void *addr, UINT32 exp)                                    \
    {                                                                                              \
        __asan_store##size((void *)addr);                                                          \
    }

DEFINE_ASAN_EXP_LOAD_STORE(1)
DEFINE_ASAN_EXP_LOAD_STORE(2)
DEFINE_ASAN_EXP_LOAD_STORE(4)
DEFINE_ASAN_EXP_LOAD_STORE(8)
DEFINE_ASAN_EXP_LOAD_STORE(16)

void __asan_exp_loadN(void *addr, SIZE_T size, UINT32 exp)
{
    __asan_loadN((void *)addr, size);
}
void __asan_exp_storeN(void *addr, SIZE_T size, UINT32 exp)
{
    __asan_storeN((void *)addr, size);
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
}

ASANAPI void __asan_unpoison_memory_region(void const volatile *addr, SIZE_T size)
{
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
    return 0;
}
void *__asan_region_is_poisoned(void *beg, SIZE_T size)
{
    return NULL;
}
