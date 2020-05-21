/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013 Thomas Skibo
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Defines for Zynq-7000 SLCR registers.
 *
 * Most of these registers are initialized by the First Stage Boot
 * Loader and are not modified by the kernel.
 *
 * Reference: Zynq-7000 All Programmable SoC Technical Reference Manual.
 * (v1.4) November 16, 2012.  Xilinx doc UG585.  SLCR register definitions
 * are in appendix B.28.
 */

#ifndef _ZY7_SLCR_H_
#define _ZY7_SLCR_H_

#define ZY7_SCLR_SCL			0x0000
#define ZY7_SLCR_LOCK			0x0004
#define   ZY7_SLCR_LOCK_MAGIC				0x767b
#define ZY7_SLCR_UNLOCK			0x0008
#define   ZY7_SLCR_UNLOCK_MAGIC				0xdf0d
#define ZY7_SLCR_LOCKSTA		0x000c

/* PLL controls. */
#define ZY7_SLCR_ARM_PLL_CTRL		0x0100
#define ZY7_SLCR_DDR_PLL_CTRL		0x0104
#define ZY7_SLCR_IO_PLL_CTRL		0x0108
#define   ZY7_SLCR_PLL_CTRL_RESET			(1 << 0)
#define   ZY7_SLCR_PLL_CTRL_PWRDWN			(1 << 1)
#define   ZY7_SLCR_PLL_CTRL_BYPASS_QUAL			(1 << 3)
#define   ZY7_SLCR_PLL_CTRL_BYPASS_FORCE		(1 << 4)
#define   ZY7_SLCR_PLL_CTRL_FDIV_SHIFT			12
#define   ZY7_SLCR_PLL_CTRL_FDIV_MASK			(0x7f << 12)
#define ZY7_SLCR_PLL_STATUS		0x010c
#define   ZY7_SLCR_PLL_STAT_ARM_PLL_LOCK		(1 << 0)
#define   ZY7_SLCR_PLL_STAT_DDR_PLL_LOCK		(1 << 1)
#define   ZY7_SLCR_PLL_STAT_IO_PLL_LOCK			(1 << 2)
#define   ZY7_SLCR_PLL_STAT_ARM_PLL_STABLE		(1 << 3)
#define   ZY7_SLCR_PLL_STAT_DDR_PLL_STABLE		(1 << 4)
#define   ZY7_SLCR_PLL_STAT_IO_PLL_STABLE		(1 << 5)
#define ZY7_SLCR_ARM_PLL_CFG		0x0110
#define ZY7_SLCR_DDR_PLL_CFG		0x0114
#define ZY7_SLCR_IO_PLL_CFG		0x0118
#define   ZY7_SLCR_PLL_CFG_RES_SHIFT			4
#define   ZY7_SLCR_PLL_CFG_RES_MASK			(0xf << 4)
#define   ZY7_SLCR_PLL_CFG_PLL_CP_SHIFT			8
#define   ZY7_SLCR_PLL_CFG_PLL_CP_MASK			(0xf << 8)
#define   ZY7_SLCR_PLL_CFG_LOCK_CNT_SHIFT		12
#define   ZY7_SLCR_PLL_CFG_LOCK_CNT_MASK		(0x3ff << 12)

/* Clock controls. */
#define ZY7_SLCR_ARM_CLK_CTRL		0x0120
#define   ZY7_SLCR_ARM_CLK_CTRL_CPU_PERI_CLKACT		(1 << 28)
#define   ZY7_SLCR_ARM_CLK_CTRL_CPU_1XCLKACT		(1 << 27)
#define   ZY7_SLCR_ARM_CLK_CTRL_CPU_2XCLKACT		(1 << 26)
#define   ZY7_SLCR_ARM_CLK_CTRL_CPU_3OR2XCLKACT		(1 << 25)
#define   ZY7_SLCR_ARM_CLK_CTRL_CPU_6OR4XCLKACT		(1 << 24)
#define   ZY7_SLCR_ARM_CLK_CTRL_SRCSEL_MASK		(3 << 4)
#define   ZY7_SLCR_ARM_CLK_CTRL_SRCSEL_ARM_PLL		(0 << 4)
#define   ZY7_SLCR_ARM_CLK_CTRL_SRCSEL_DDR_PLL		(2 << 4)
#define   ZY7_SLCR_ARM_CLK_CTRL_SRCSEL_IO_PLL		(3 << 4)
#define   ZY7_SLCR_ARM_CLK_CTRL_DIVISOR_SHIFT		8
#define   ZY7_SLCR_ARM_CLK_CTRL_DIVISOR_MASK		(0x3f << 8)
#define ZY7_SLCR_DDR_CLK_CTRL		0x0124
#define   ZY7_SLCR_DDR_CLK_CTRL_2XCLK_DIV_SHIFT		26
#define   ZY7_SLCR_DDR_CLK_CTRL_2XCLK_DIV_MASK		(0x3f << 26)
#define   ZY7_SLCR_DDR_CLK_CTRL_3XCLK_DIV_SHIFT		20
#define   ZY7_SLCR_DDR_CLK_CTRL_3XCLK_DIV_MASK		(0x3f << 20)
#define   ZY7_SLCR_DDR_CLK_CTRL_2XCLKACT		(1 << 1)
#define   ZY7_SLCR_DDR_CLK_CTRL_3XCLKACT		(1 << 0)
#define ZY7_SLCR_DCI_CLK_CTRL		0x0128
#define   ZY7_SLCR_DCI_CLK_CTRL_DIVISOR1_SHIFT		20
#define   ZY7_SLCR_DCI_CLK_CTRL_DIVISOR1_MASK		(0x3f << 20)
#define   ZY7_SLCR_DCI_CLK_CTRL_DIVISOR0_SHIFT		8
#define   ZY7_SLCR_DCI_CLK_CTRL_DIVISOR0_MASK		(0x3f << 8)
#define   ZY7_SLCR_DCI_CLK_CTRL_CLKACT			(1 << 0)
#define ZY7_SLCR_APER_CLK_CTRL		0x012c	/* amba periph clk ctrl */
#define   ZY7_SLCR_APER_CLK_CTRL_SMC_CPU_1XCLKACT	(1 << 24)
#define   ZY7_SLCR_APER_CLK_CTRL_LQSPI_CPU_1XCLKACT	(1 << 23)
#define   ZY7_SLCR_APER_CLK_CTRL_GPIO_CPU_1XCLKACT	(1 << 22)
#define   ZY7_SLCR_APER_CLK_CTRL_UART1_CPU_1XCLKACT	(1 << 21)
#define   ZY7_SLCR_APER_CLK_CTRL_UART0_CPU_1XCLKACT	(1 << 20)
#define   ZY7_SLCR_APER_CLK_CTRL_I2C1_CPU_1XCLKACT	(1 << 19)
#define   ZY7_SLCR_APER_CLK_CTRL_I2C0_CPU_1XCLKACT	(1 << 18)
#define   ZY7_SLCR_APER_CLK_CTRL_CAN1_CPU_1XCLKACT	(1 << 17)
#define   ZY7_SLCR_APER_CLK_CTRL_CAN0_CPU_1XCLKACT	(1 << 16)
#define   ZY7_SLCR_APER_CLK_CTRL_SPI1_CPU_1XCLKACT	(1 << 15)
#define   ZY7_SLCR_APER_CLK_CTRL_SPI0_CPU_1XCLKACT	(1 << 14)
#define   ZY7_SLCR_APER_CLK_CTRL_SDI1_CPU_1XCLKACT	(1 << 11)
#define   ZY7_SLCR_APER_CLK_CTRL_SDI0_CPU_1XCLKACT	(1 << 10)
#define   ZY7_SLCR_APER_CLK_CTRL_GEM1_CPU_1XCLKACT	(1 << 7)
#define   ZY7_SLCR_APER_CLK_CTRL_GEM0_CPU_1XCLKACT	(1 << 6)
#define   ZY7_SLCR_APER_CLK_CTRL_USB1_CPU_1XCLKACT	(1 << 3)
#define   ZY7_SLCR_APER_CLK_CTRL_USB0_CPU_1XCLKACT	(1 << 2)
#define   ZY7_SLCR_APER_CLK_CTRL_DMA_CPU_1XCLKACT	(1 << 0)
#define ZY7_SLCR_USB0_CLK_CTRL		0x0130
#define ZY7_SLCR_USB1_CLK_CTRL		0x0134
#define ZY7_SLCR_GEM0_RCLK_CTRL		0x0138
#define ZY7_SLCR_GEM1_RCLK_CTRL		0x013c
#define ZY7_SLCR_GEM0_CLK_CTRL		0x0140
#define ZY7_SLCR_GEM1_CLK_CTRL		0x0144
#define   ZY7_SLCR_GEM_CLK_CTRL_DIVISOR1_MASK		(0x3f << 20)
#define   ZY7_SLCR_GEM_CLK_CTRL_DIVISOR1_SHIFT		20
#define   ZY7_SLCR_GEM_CLK_CTRL_DIVISOR1_MAX		0x3f
#define   ZY7_SLCR_GEM_CLK_CTRL_DIVISOR_MASK		(0x3f << 8)
#define   ZY7_SLCR_GEM_CLK_CTRL_DIVISOR_SHIFT		8
#define   ZY7_SLCR_GEM_CLK_CTRL_DIVISOR_MAX		0x3f
#define   ZY7_SLCR_GEM_CLK_CTRL_SRCSEL_MASK		(7 << 4)
#define   ZY7_SLCR_GEM_CLK_CTRL_SRCSEL_IO_PLL		(0 << 4)
#define   ZY7_SLCR_GEM_CLK_CTRL_SRCSEL_ARM_PLL		(2 << 4)
#define   ZY7_SLCR_GEM_CLK_CTRL_SRCSEL_DDR_PLL		(3 << 4)
#define   ZY7_SLCR_GEM_CLK_CTRL_SRCSEL_EMIO_CLK		(4 << 4)
#define   ZY7_SLCR_GEM_CLK_CTRL_CLKACT			1
#define ZY7_SLCR_SMC_CLK_CTRL		0x0148
#define ZY7_SLCR_LQSPI_CLK_CTRL		0x014c
#define ZY7_SLCR_SDIO_CLK_CTRL		0x0150
#define ZY7_SLCR_UART_CLK_CTRL		0x0154
#define ZY7_SLCR_SPI_CLK_CTRL		0x0158
#define ZY7_SLCR_CAN_CLK_CTRL		0x015c
#define ZY7_SLCR_CAN_MIOCLK_CTRL	0x0160
#define ZY7_SLCR_DBG_CLK_CTRL		0x0164
#define ZY7_SLCR_PCAP_CLK_CTRL		0x0168
#define ZY7_SLCR_TOPSW_CLK_CTRL		0x016c	/* central intercnn clk ctrl */
#define ZY7_SLCR_FPGA_CLK_CTRL(unit)	(0x0170 + 0x10 * (unit))
#define	  ZY7_SLCR_FPGA_CLK_CTRL_DIVISOR1_SHIFT		20
#define	  ZY7_SLCR_FPGA_CLK_CTRL_DIVISOR1_MASK		(0x3f << 20)
#define	  ZY7_SLCR_FPGA_CLK_CTRL_DIVISOR0_SHIFT		8
#define	  ZY7_SLCR_FPGA_CLK_CTRL_DIVISOR0_MASK		(0x3f << 8)
#define	  ZY7_SLCR_FPGA_CLK_CTRL_DIVISOR_MAX		0x3f
#define	  ZY7_SLCR_FPGA_CLK_CTRL_SRCSEL_SHIFT		4
#define	  ZY7_SLCR_FPGA_CLK_CTRL_SRCSEL_MASK		(3 << 4)
#define ZY7_SLCR_FPGA_THR_CTRL(unit)	(0x0174 + 0x10 * (unit))
#define ZY7_SLCR_FPGA_THR_CTRL_CNT_RST			(1 << 1)
#define ZY7_SLCR_FPGA_THR_CTRL_CPU_START		(1 << 0)
#define ZY7_SLCR_FPGA_THR_CNT(unit)	(0x0178 + 0x10 * (unit))
#define ZY7_SLCR_FPGA_THR_STA(unit)	(0x017c + 0x10 * (unit))
#define ZY7_SLCR_CLK_621_TRUE		0x01c4	/* cpu clock ratio mode */

/* Reset controls. */
#define ZY7_SLCR_PSS_RST_CTRL		0x0200
#define   ZY7_SLCR_PSS_RST_CTRL_SOFT_RESET		(1 << 0)
#define ZY7_SLCR_DDR_RST_CTRL		0x0204
#define ZY7_SLCR_TOPSW_RST_CTRL		0x0208
#define ZY7_SLCR_DMAC_RST_CTRL		0x020c
#define ZY7_SLCR_USB_RST_CTRL		0x0210
#define ZY7_SLCR_GEM_RST_CTRL		0x0214
#define ZY7_SLCR_SDIO_RST_CTRL		0x0218
#define ZY7_SLCR_SPI_RST_CTRL		0x021c
#define ZY7_SLCR_CAN_RST_CTRL		0x0220
#define ZY7_SLCR_I2C_RST_CTRL		0x0224
#define ZY7_SLCR_UART_RST_CTRL		0x0228
#define ZY7_SLCR_GPIO_RST_CTRL		0x022c
#define ZY7_SLCR_LQSPI_RST_CTRL		0x0230
#define ZY7_SLCR_SMC_RST_CTRL		0x0234
#define ZY7_SLCR_OCM_RST_CTRL		0x0238
#define ZY7_SLCR_DEVCI_RST_CTRL		0x023c
#define ZY7_SLCR_FPGA_RST_CTRL		0x0240
#define   ZY7_SLCR_FPGA_RST_CTRL_FPGA3_OUT_RST		(1 << 3)
#define   ZY7_SLCR_FPGA_RST_CTRL_FPGA2_OUT_RST		(1 << 2)
#define   ZY7_SLCR_FPGA_RST_CTRL_FPGA1_OUT_RST		(1 << 1)
#define   ZY7_SLCR_FPGA_RST_CTRL_FPGA0_OUT_RST		(1 << 0)
#define   ZY7_SLCR_FPGA_RST_CTRL_RST_ALL		0xf
#define ZY7_SLCR_A9_CPU_RST_CTRL	0x0244
#define ZY7_SLCR_RS_AWDT_CTRL		0x024c

#define ZY7_SLCR_REBOOT_STAT		0x0258
#define   ZY7_SLCR_REBOOT_STAT_STATE_MASK		(0xffU << 24)
#define   ZY7_SLCR_REBOOT_STAT_POR			(1 << 22)
#define   ZY7_SLCR_REBOOT_STAT_SRST_B			(1 << 21)
#define   ZY7_SLCR_REBOOT_STAT_DBG_RST			(1 << 20)
#define   ZY7_SLCR_REBOOT_STAT_SLC_RST			(1 << 19)
#define   ZY7_SLCR_REBOOT_STAT_AWDT1_RST		(1 << 18)
#define   ZY7_SLCR_REBOOT_STAT_AWDT0_RST		(1 << 17)
#define   ZY7_SLCR_REBOOT_STAT_SWDT_RST			(1 << 16)
#define   ZY7_SLCR_REBOOT_STAT_BOOTROM_ERR_CODE_MASK	(0xffff)
#define ZY7_SLCR_BOOT_MODE		0x025c
#define   ZY7_SLCR_BOOT_MODE_PLL_BYPASS			(1 << 4)
#define   ZY7_SLCR_BOOT_MODE_JTAG_INDEP			(1 << 3)
#define   ZY7_SLCR_BOOT_MODE_BOOTDEV_MASK		7
#define   ZY7_SLCR_BOOT_MODE_BOOTDEV_JTAG		0
#define   ZY7_SLCR_BOOT_MODE_BOOTDEV_QUAD_SPI		1
#define   ZY7_SLCR_BOOT_MODE_BOOTDEV_NOR		2
#define   ZY7_SLCR_BOOT_MODE_BOOTDEV_NAND		4
#define   ZY7_SLCR_BOOT_MODE_BOOTDEV_SD_CARD		5
#define ZY7_SLCR_APU_CTRL		0x0300
#define ZY7_SLCR_WDT_CLK_SEL		0x0304

#define ZY7_SLCR_PSS_IDCODE		0x0530
#define   ZY7_SLCR_PSS_IDCODE_REVISION_MASK		(0xfU << 28)
#define   ZY7_SLCR_PSS_IDCODE_REVISION_SHIFT		28
#define   ZY7_SLCR_PSS_IDCODE_FAMILY_MASK		(0x7f << 21)
#define   ZY7_SLCR_PSS_IDCODE_FAMILY_SHIFT		21
#define   ZY7_SLCR_PSS_IDCODE_SUB_FAMILY_MASK		(0xf << 17)
#define   ZY7_SLCR_PSS_IDCODE_SUB_FAMILY_SHIFT		17
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_MASK		(0x1f << 12)
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_7Z007S		(0x03 << 12)
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_7Z010		(0x02 << 12)
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_7Z012S		(0x1c << 12)
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_7Z014S		(0x08 << 12)
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_7Z015		(0x1b << 12)
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_7Z020		(0x07 << 12)
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_7Z030		(0x0c << 12)
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_7Z045		(0x11 << 12)
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_7Z100		(0x16 << 12)
#define   ZY7_SLCR_PSS_IDCODE_DEVICE_SHIFT		12
#define   ZY7_SLCR_PSS_IDCODE_MNFR_ID_MASK		(0x7ff << 1)
#define   ZY7_SLCR_PSS_IDCODE_MNFR_ID_SHIFT		1

#define ZY7_SLCR_DDR_URGENT		0x0600
#define ZY7_SLCR_DDR_CAL_START		0x060c
#define ZY7_SLCR_DDR_REF_START		0x0614
#define ZY7_SLCR_DDR_CMD_STA		0x0618
#define ZY7_SLCR_DDR_URGENT_SEL		0x061c
#define ZY7_SLCR_DDR_DFI_STATUS		0x0620

/* MIO Pin controls */
#define ZY7_SLCR_MIO_PIN(n)		(0x0700 + (n) * 4)	/* 0-53 */
#define   ZY7_SLCR_MIO_PIN_RCVR_DIS			(1 << 13)
#define   ZY7_SLCR_MIO_PIN_PULLUP_EN			(1 << 12)
#define   ZY7_SLCR_MIO_PIN_IO_TYPE_MASK			(7 << 9)
#define   ZY7_SLCR_MIO_PIN_IO_TYPE_LVTTL		(0 << 9)
#define   ZY7_SLCR_MIO_PIN_IO_TYPE_LVCMOS18		(1 << 9)
#define   ZY7_SLCR_MIO_PIN_IO_TYPE_LVCMOS25		(2 << 9)
#define   ZY7_SLCR_MIO_PIN_IO_TYPE_LVCMOS33		(3 << 9)
#define   ZY7_SLCR_MIO_PIN_IO_TYPE_HSTL			(4 << 9)
#define   ZY7_SLCR_MIO_PIN_L2_SEL_MASK			(3 << 3)
#define   ZY7_SLCR_MIO_PIN_L2_SEL_L3_MUX		(0 << 3)
#define   ZY7_SLCR_MIO_PIN_L2_SEL_SRAM_NOR_CS0		(1 << 3)
#define   ZY7_SLCR_MIO_PIN_L2_SEL_NAND_CS		(2 << 3)
#define   ZY7_SLCR_MIO_PIN_L2_SEL_SDIO0_PC		(3 << 3)
#define   ZY7_SLCR_MIO_PIN_L1_SEL			(1 << 2)
#define   ZY7_SLCR_MIO_PIN_L0_SEL			(1 << 1)
#define   ZY7_SLCR_MIO_PIN_TRI_EN			(1 << 0)

#define ZY7_SLCR_MIO_LOOPBACK		0x0804
#define   ZY7_SLCR_MIO_LOOPBACK_I2C0_I2C1		(1 << 3)
#define   ZY7_SLCR_MIO_LOOPBACK_CAN0_CAN1		(1 << 2)
#define   ZY7_SLCR_MIO_LOOPBACK_UA0_UA1			(1 << 1)
#define   ZY7_SLCR_MIO_LOOPBACK_SPI0_SPI1		(1 << 0)
#define ZY7_SLCR_MIO_MST_TRI0		0x080c
#define ZY7_SLCR_MIO_MST_TRI1		0x0810
#define ZY7_SLCR_SD0_WP_CD_SEL		0x0830
#define ZY7_SLCR_SD1_WP_CD_SEL		0x0834

/* PS-PL level shifter control. */
#define ZY7_SLCR_LVL_SHFTR_EN		0x900
#define   ZY7_SLCR_LVL_SHFTR_EN_USER_LVL_IN_EN_0	(1 << 3) /* PL to PS */
#define   ZY7_SLCR_LVL_SHFTR_EN_USER_LVL_OUT_EN_0	(1 << 2) /* PS to PL */
#define   ZY7_SLCR_LVL_SHFTR_EN_USER_LVL_IN_EN_1	(1 << 1) /* PL to PS */
#define   ZY7_SLCR_LVL_SHFTR_EN_USER_LVL_OUT_EN_1	(1 << 0) /* PS to PL */
#define   ZY7_SLCR_LVL_SHFTR_EN_ALL			0xf

#define ZY7_SLCR_OCM_CFG		0x0910

#define ZY7_SLCR_GPIOB_CTRL		0x0b00
#define ZY7_SLCR_GPIOB_CFG_CMOS18	0x0b04
#define ZY7_SLCR_GPIOB_CFG_CMOS25	0x0b08
#define ZY7_SLCR_GPIOB_CFG_CMOS33	0x0b0c
#define ZY7_SLCR_GPIOB_CFG_LVTTL	0x0b10
#define ZY7_SLCR_GPIOB_CFG_HSTL		0x0b14
#define ZY7_SLCR_GPIOB_DRVR_BIAS_CTRL	0x0b18

#define ZY7_SLCR_DDRIOB_ADDR0		0x0b40
#define ZY7_SLCR_DDRIOB_ADDR1		0x0b44
#define ZY7_SLCR_DDRIOB_DATA0		0x0b48
#define ZY7_SLCR_DDRIOB_DATA1		0x0b4c
#define ZY7_SLCR_DDRIOB_DIFF0		0x0b50
#define ZY7_SLCR_DDRIOB_DIFF1		0x0b54
#define ZY7_SLCR_DDRIOB_CLK		0x0b58
#define ZY7_SLCR_DDRIOB_DRIVE_SLEW_ADDR	0x0b5c
#define ZY7_SLCR_DDRIOB_DRIVE_SLEW_DATA	0x0b60
#define ZY7_SLCR_DDRIOB_DRIVE_SLEW_DIFF	0x0b64
#define ZY7_SLCR_DDRIOB_DRIVE_SLEW_CLK	0x0b68
#define ZY7_SLCR_DDRIOB_DDR_CTRL	0x0b6c
#define ZY7_SLCR_DDRIOB_DCI_CTRL	0x0b70
#define ZY7_SLCR_DDRIOB_DCI_STATUS	0x0b74

#ifdef _KERNEL
extern void zy7_slcr_preload_pl(void);
extern void zy7_slcr_postload_pl(int en_level_shifters);
extern int cgem_set_ref_clk(int unit, int frequency);

/* Should be consistent with SRCSEL field of FPGAx_CLK_CTRL */
#define	ZY7_PL_FCLK_SRC_IO	0
#define	ZY7_PL_FCLK_SRC_IO_ALT	1 /* ZY7_PL_FCLK_SRC_IO is b0x */
#define	ZY7_PL_FCLK_SRC_ARM	2
#define	ZY7_PL_FCLK_SRC_DDR	3

int zy7_pl_fclk_set_source(int unit, int source);
int zy7_pl_fclk_get_source(int unit);
int zy7_pl_fclk_set_freq(int unit, int freq);
int zy7_pl_fclk_get_freq(int unit);
int zy7_pl_fclk_enable(int unit);
int zy7_pl_fclk_disable(int unit);
int zy7_pl_fclk_enabled(int unit);
int zy7_pl_level_shifters_enabled(void);
void zy7_pl_level_shifters_enable(void);
void zy7_pl_level_shifters_disable(void);

#endif
#endif /* _ZY7_SLCR_H_ */
