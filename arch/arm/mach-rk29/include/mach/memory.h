/* arch/arm/mach-rk29/include/mach/memory.h
 *
 * Copyright (C) 2010 ROCKCHIP, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_RK29_MEMORY_H
#define __ASM_ARCH_RK29_MEMORY_H

/* physical offset of RAM */
#define PHYS_OFFSET		UL(0x60000000)

#define CONSISTENT_DMA_SIZE	SZ_8M

/*
 * SRAM memory whereabouts
 */
#define SRAM_CODE_OFFSET	0xFEF00000
#define SRAM_CODE_END		0xFEF01FFF
#define SRAM_DATA_OFFSET	0xFEF02000
#define SRAM_DATA_END		0xFEF03FFF

#endif

