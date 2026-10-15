/* Entry stub, first bytes of every payload.
 *
 * Recovery's `run` jumps to SOC_PAYLOAD_BASE without flushing caches. When a payload follows another one at
 * the same address, the CPU may still hold the previous payload's instructions (I-cache) and constants
 * (D-cache, literal pools, the nonce word). The result was a crash right after `run` on nearly every second
 * send of a session (observed 2026-10-15).
 *
 * This stub is byte-identical in every payload (same code, same position, a relative branch of constant
 * length), so a stale copy of it is the same as a fresh one. It cleans and invalidates the data cache over
 * the payload area, invalidates the instruction cache and the branch predictor, synchronises, then continues
 * into payload_main, which is placed right after it by payload.ld. Only r0 and r1 are used; lr is untouched.
 */
#include "hal/hal.h"

#define FLUSH_BYTES 0x4000   /* payload code, literals and the host-appended nonce */

__asm__(
	".section .text.entry,\"ax\",%progbits\n"
	".thumb\n"
	".syntax unified\n"
	".global payload_entry\n"
	".type payload_entry,%function\n"
	"payload_entry:\n"
	"	movw	r0, #:lower16:" STR(SOC_PAYLOAD_BASE) "\n"
	"	movt	r0, #:upper16:" STR(SOC_PAYLOAD_BASE) "\n"
	"	movw	r1, #" STR(FLUSH_BYTES) "\n"
	"1:	mcr	p15, 0, r0, c7, c14, 1\n"        /* clean and invalidate D-cache line by address */
	"	add	r0, r0, #32\n"
	"	subs	r1, r1, #32\n"
	"	bne	1b\n"
	"	dsb	sy\n"
	"	mov	r0, #0\n"
	"	mcr	p15, 0, r0, c7, c5, 0\n"          /* invalidate all instruction caches */
	"	mcr	p15, 0, r0, c7, c5, 6\n"          /* invalidate branch predictor */
	"	dsb	sy\n"
	"	isb	sy\n"
	"	b.w	payload_main\n"
	".previous\n"
);
