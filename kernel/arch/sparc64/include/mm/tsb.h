/*
 * Copyright (C) 2006 Jakub Jermar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup sparc64mm	
 * @{
 */
/** @file
 */

#ifndef KERN_sparc64_TSB_H_
#define KERN_sparc64_TSB_H_

/*
 * ITSB abd DTSB will claim 64K of memory, which
 * is a nice number considered that it is one of
 * the page sizes supported by hardware, which,
 * again, is nice because TSBs need to be locked
 * in TLBs - only one TLB entry will do.
 */
#define TSB_SIZE			2	/* when changing this, change
						 * as.c as well */
#define ITSB_ENTRY_COUNT		(512 * (1 << TSB_SIZE))
#define DTSB_ENTRY_COUNT		(512 * (1 << TSB_SIZE))

#define TSB_TAG_TARGET_CONTEXT_SHIFT	48

#ifndef __ASM__

#include <arch/mm/tte.h>
#include <arch/mm/mmu.h>
#include <arch/types.h>
#include <typedefs.h>

/** TSB Tag Target register. */
union tsb_tag_target {
	uint64_t value;
	struct {
		unsigned invalid : 1;	/**< Invalidated by software. */
		unsigned : 2;
		unsigned context : 13;	/**< Software ASID. */
		unsigned : 6;
		uint64_t va_tag : 42;	/**< Virtual address bits <63:22>. */
	} __attribute__ ((packed));
};
typedef union tsb_tag_target tsb_tag_target_t;

/** TSB entry. */
struct tsb_entry {
	tsb_tag_target_t tag;
	tte_data_t data;
} __attribute__ ((packed));
typedef struct tsb_entry tsb_entry_t;

/** TSB Base register. */
union tsb_base_reg {
	uint64_t value;
	struct {
		uint64_t base : 51;	/**< TSB base address, bits 63:13. */
		unsigned split : 1;	/**< Split vs. common TSB for 8K and 64K
					 * pages. HelenOS uses only 8K pages
					 * for user mappings, so we always set
					 * this to 0.
					 */
		unsigned : 9;
		unsigned size : 3;	/**< TSB size. Number of entries is
					 * 512 * 2^size. */
	} __attribute__ ((packed));
};
typedef union tsb_base_reg tsb_base_reg_t;

/** Read ITSB Base register.
 *
 * @return Content of the ITSB Base register.
 */
static inline uint64_t itsb_base_read(void)
{
	return asi_u64_read(ASI_IMMU, VA_IMMU_TSB_BASE);
}

/** Read DTSB Base register.
 *
 * @return Content of the DTSB Base register.
 */
static inline uint64_t dtsb_base_read(void)
{
	return asi_u64_read(ASI_DMMU, VA_DMMU_TSB_BASE);
}

/** Write ITSB Base register.
 *
 * @param v New content of the ITSB Base register.
 */
static inline void itsb_base_write(uint64_t v)
{
	asi_u64_write(ASI_IMMU, VA_IMMU_TSB_BASE, v);
}

/** Write DTSB Base register.
 *
 * @param v New content of the DTSB Base register.
 */
static inline void dtsb_base_write(uint64_t v)
{
	asi_u64_write(ASI_DMMU, VA_DMMU_TSB_BASE, v);
}

extern void tsb_invalidate(as_t *as, uintptr_t page, count_t pages);
extern void itsb_pte_copy(pte_t *t);
extern void dtsb_pte_copy(pte_t *t, bool ro);

#endif /* !def __ASM__ */

#endif

/** @}
 */
