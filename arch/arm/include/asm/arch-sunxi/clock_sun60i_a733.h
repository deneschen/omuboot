/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _SUNXI_CLOCK_SUN60I_A733_H
#define _SUNXI_CLOCK_SUN60I_A733_H

#include <linux/bitops.h>

#define CCU_MMC0_CLK_CFG		0x0d00
#define CCU_MMC1_CLK_CFG		0x0d10
#define CCU_MMC2_CLK_CFG		0x0d20
#define CCU_MMC3_CLK_CFG		0x0d30

#define CCM_MMC_CTRL_M(x)		((x) - 1)
#define CCM_MMC_CTRL_N(x)		((x) << 8)
#define CCM_MMC_CTRL_OSCM24		(0x0 << 24)
#define CCM_MMC_CTRL_PLL6		(0x1 << 24)
#define CCM_MMC_CTRL_ENABLE		BIT(31)
#define CCM_MMC_CTRL_OCLK_DLY(x)	0
#define CCM_MMC_CTRL_SCLK_DLY(x)	0
#define CCM_MMC_CTRL_MODE_SEL_NEW	0

#define RESET_SHIFT			16

#ifndef __ASSEMBLY__
static inline unsigned int clock_get_pll6(void)
{
	return 400000000;
}
#endif

#endif /* _SUNXI_CLOCK_SUN60I_A733_H */
