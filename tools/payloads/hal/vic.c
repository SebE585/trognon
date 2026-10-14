/* Interrupt controller layer: IRQ stub, handler and vector hook. */
#include "hal/hal.h"
#include "hal/vic.h"

/* Called from the IRQ stub. All VIC0 sources but ours are disabled while the test runs, so a hit on
 * another bit is a surprise: count it and shut every VIC up so it cannot storm. */
void irq_c(void)
{
	u32 st = VICREG(SOC_VIC0, 0x000);
	SC->irq_status = st;
	if (st & SC->irq_bit) {
		SC->irq_vec = VICREG(SOC_VIC0, 0xF00);        /* read = "being serviced" */
		VICREG(SOC_VIC0, 0x01C) = SC->irq_bit;        /* SOFTINTCLEAR */
		VICREG(SOC_VIC0, 0x014) = SC->irq_bit;        /* INTENCLEAR */
		VICREG(SOC_VIC0, 0xF00) = 0;                  /* end of interrupt */
		SC->irq = SC->irq + 1;
	} else {
		VICREG(SOC_VIC0, 0x014) = 0xFFFFFFFF;
		VICREG(SOC_VIC1, 0x014) = 0xFFFFFFFF;
		VICREG(SOC_VIC2, 0x014) = 0xFFFFFFFF;
		SC->stray = SC->stray + 1;
	}
}

/* IRQ entry: LR_irq = interrupted PC + 4, SPSR = interrupted CPSR. Own stack, save everything,
 * return to the interrupted instruction with CPSR restored (ldm ... ^). */
__attribute__((naked, target("arm"))) void exc_irq(void)
{
	asm volatile(
		"sub lr, lr, #4\n"
		"movw sp, #:lower16:" STR(SOC_IRQ_STACK_TOP) "\n"
		"movt sp, #:upper16:" STR(SOC_IRQ_STACK_TOP) "\n"
		"push {r0-r12, lr}\n"
		"bl irq_c\n"
		"ldmfd sp!, {r0-r12, pc}^\n");
}

void hook_irq(int on)
{
	u32 addr = SOC_VEC_IRQ;
	asm volatile("" : "+r"(addr));          /* hide the constant: GCC would flag a write near address 0 */
	volatile u32 *vec = (volatile u32 *)addr;
	if (on)
		SC->old[3] = *vec;
	*vec = on ? (u32)exc_irq : SC->old[3];
}
