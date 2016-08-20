/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KERNEL_CPU_TSC_HEADER
#define KERNEL_CPU_TSC_HEADER   1

#include <grub/types.h>
#include <grub/i386/cpuid.h>

void grub_tsc_init (void);
/* In ms per 2^32 ticks.  */
extern grub_uint32_t EXPORT_VAR(grub_tsc_rate);

#endif /* ! KERNEL_CPU_TSC_HEADER */