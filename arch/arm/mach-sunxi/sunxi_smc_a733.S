/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * AArch32 SMC wrapper for A733. Preserve the full caller state around the
 * monitor call, matching the vendor __sunxi_smc_call ABI.
 */

#include <linux/linkage.h>

	.arch	armv7-a
	.arch_extension sec
	.arm

ENTRY(sunxi_smc_call4)
	stmfd	sp!, {r4-r12, lr}
	.arch_extension sec
	smc	#0
	ldmfd	sp!, {r4-r12, pc}
ENDPROC(sunxi_smc_call4)
