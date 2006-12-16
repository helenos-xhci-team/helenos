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

/** @addtogroup sparc64	
 * @{
 */
/** @file
 */

#include <smp/ipi.h>
#include <cpu.h>
#include <arch/cpu.h>
#include <arch/asm.h>
#include <config.h>
#include <mm/tlb.h>
#include <arch/interrupt.h>
#include <arch/trap/interrupt.h>
#include <arch/barrier.h>
#include <preemption.h>
#include <time/delay.h>
#include <panic.h>

/** Invoke function on another processor.
 *
 * Currently, only functions without arguments are supported.
 * Supporting more arguments in the future should be no big deal.
 *
 * Interrupts must be disabled prior to this call.
 *
 * @param mid MID of the target processor.
 * @param func Function to be invoked.
 */
static void cross_call(int mid, void (* func)(void))
{
	uint64_t status;
	bool done;

	/*
	 * This function might enable interrupts for a while.
	 * In order to prevent migration to another processor,
	 * we explicitly disable preemption.
	 */
	
	preemption_disable();
	
	status = asi_u64_read(ASI_INTR_DISPATCH_STATUS, 0);
	if (status & INTR_DISPATCH_STATUS_BUSY)
		panic("Interrupt Dispatch Status busy bit set\n");
	
	do {
		asi_u64_write(ASI_UDB_INTR_W, ASI_UDB_INTR_W_DATA_0, (uintptr_t) func);
		asi_u64_write(ASI_UDB_INTR_W, ASI_UDB_INTR_W_DATA_1, 0);
		asi_u64_write(ASI_UDB_INTR_W, ASI_UDB_INTR_W_DATA_2, 0);
		asi_u64_write(ASI_UDB_INTR_W, (mid << INTR_VEC_DISPATCH_MID_SHIFT) | ASI_UDB_INTR_W_DISPATCH, 0);
	
		membar();
		
		do {
			status = asi_u64_read(ASI_INTR_DISPATCH_STATUS, 0);
		} while (status & INTR_DISPATCH_STATUS_BUSY);
		
		done = !(status & INTR_DISPATCH_STATUS_NACK);
		if (!done) {
			/*
			 * Prevent deadlock.
			 */			
			(void) interrupts_enable();
			delay(20 + (tick_read() & 0xff));
			(void) interrupts_disable();
		}
	} while (done);
	
	preemption_enable();
}

/*
 * Deliver IPI to all processors except the current one.
 *
 * The sparc64 architecture does not support any group addressing
 * which is found, for instance, on ia32 and amd64. Therefore we
 * need to simulate the broadcast by sending the message to
 * all target processors step by step.
 *
 * We assume that interrupts are disabled.
 *
 * @param ipi IPI number.
 */
void ipi_broadcast_arch(int ipi)
{
	int i;
	
	void (* func)(void);
	
	switch (ipi) {
	case IPI_TLB_SHOOTDOWN:
		func = tlb_shootdown_ipi_recv;
		break;
	default:
		panic("Unknown IPI (%d).\n", ipi);
		break;
	}
	
	/*
	 * As long as we don't support hot-plugging
	 * or hot-unplugging of CPUs, we can walk
	 * the cpus array and read processor's MID
	 * without locking.
	 */
	
	for (i = 0; i < config.cpu_active; i++) {
		if (&cpus[i] == CPU)
			continue;		/* skip the current CPU */

		cross_call(cpus[i].arch.mid, func);
	}
}

/** @}
 */
