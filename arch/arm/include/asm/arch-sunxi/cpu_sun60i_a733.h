/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Allwinner A733 (sun60iw2) address map used by the BL33-only port.
 */

#ifndef _SUNXI_CPU_SUN60I_A733_H
#define _SUNXI_CPU_SUN60I_A733_H

#define SUNXI_CCM_BASE			0x02002000
#define SUNXI_TIMER_BASE		0x03009000

#define SUNXI_TWI0_BASE			0x02510000
#define SUNXI_TWI1_BASE			0x02511000
#define SUNXI_TWI2_BASE			0x02512000
#define SUNXI_TWI3_BASE			0x02513000

#define SUNXI_SRAMC_BASE		0x00040000
#define SUNXI_SIDC_BASE			0x03006000
#define SUNXI_SID_BASE			0x03006000
#define SUNXI_GIC600_BASE		0x03400000

#define SUNXI_MMC0_BASE			0x04020000
#define SUNXI_MMC1_BASE			0x04021000
#define SUNXI_MMC2_BASE			0x04022000
#define SUNXI_MMC3_BASE			0x04023000

#define SUNXI_PRCM_BASE			0x07010000
#define SUNXI_R_WDOG_BASE		0x07021000
#define SUNXI_R_TWI_BASE		0x07083000
#define SUNXI_RTC_BASE			0x07090000

#ifndef __ASSEMBLY__
void sunxi_board_init(void);
void sunxi_reset(void);
int sunxi_get_sid(unsigned int *sid);
#endif

#endif /* _SUNXI_CPU_SUN60I_A733_H */
