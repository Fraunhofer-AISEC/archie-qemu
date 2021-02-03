/*
 * STM32F052 SoC
 *
 * Copyright (c) 2014 Alistair Francis <alistair@alistair23.me>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef HW_ARM_STM32F052_SOC_H
#define HW_ARM_STM32F052_SOC_H

//#include "hw/misc/stm32f2xx_syscfg.h"
//#include "hw/timer/stm32f2xx_timer.h"
#include "hw/char/stm32f0xx_usart.h"
//#include "hw/adc/stm32f2xx_adc.h"
#include "hw/or-irq.h"
//#include "hw/ssi/stm32f2xx_spi.h"
#include "hw/arm/armv7m.h"

#define TYPE_STM32F052_SOC "stm32f052-soc"
#define STM32F052_SOC(obj) \
    OBJECT_CHECK(STM32F052State, (obj), TYPE_STM32F052_SOC)

#define STM_NUM_USARTS 8
//#define STM_NUM_TIMERS 4
//#define STM_NUM_ADCS 3
//#define STM_NUM_SPIS 3

#define FLASH_BASE_ADDRESS 0x08000000
#define FLASH_SIZE (64 * 1024)
#define SRAM_BASE_ADDRESS 0x20000000
#define SRAM_SIZE (8 * 1024)

typedef struct STM32F052State {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    char *cpu_type;

    ARMv7MState armv7m;

//   STM32F2XXSyscfgState syscfg;
    STM32F0XXUsartState usart[STM_NUM_USARTS];
//    STM32F2XXTimerState timer[STM_NUM_TIMERS];
//    STM32F2XXADCState adc[STM_NUM_ADCS];
//    STM32F2XXSPIState spi[STM_NUM_SPIS];

    qemu_or_irq *adc_irqs;
} STM32F052State;

#endif
