/*
 * STM32F0XX USART
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

#ifndef HW_STM32F0XX_USART_H
#define HW_STM32F0XX_USART_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"

#define USART_CR1   0x00
#define USART_CR2   0x04
#define USART_CR3  0x08
#define USART_BRR  0x0C
#define USART_GTPR  0x10
#define USART_RTOR  0x14
#define USART_RQR 0x18
#define USART_ISR 0x1C
#define USART_ICR 0x20
#define USART_RDR 0x24
#define USART_TDR 0x28

/*
 * RM0091
 * Looking at 27.8.7, it seems it
 * should be 0x020000c0, and that's how real hardware behaves.
 */
#define USART_ISR_RESET (USART_ISR_TXE | USART_ISR_TC | 0x02000000)

#define USART_ISR_TXE  (1 << 7)
#define USART_ISR_TC   (1 << 6)
#define USART_ISR_RXNE (1 << 5)

#define USART_CR1_UE  (1 << 1)
#define USART_CR1_RXNEIE  (1 << 5)
#define USART_CR1_TE  (1 << 3)
#define USART_CR1_RE  (1 << 2)

#define TYPE_STM32F0XX_USART "stm32f0xx-usart"
#define STM32F0XX_USART(obj) \
    OBJECT_CHECK(STM32F0XXUsartState, (obj), TYPE_STM32F0XX_USART)

typedef struct {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion mmio;

    uint32_t usart_cr1;
    uint32_t usart_cr2;
    uint32_t usart_cr3;
    uint32_t usart_brr;
    uint32_t usart_gtpr;
    uint32_t usart_rtor;
    uint32_t usart_rqr;
    uint32_t usart_isr;
    uint32_t usart_icr;
    uint32_t usart_rdr;
    uint32_t usart_tdr;

    CharBackend chr;
    qemu_irq irq;
} STM32F0XXUsartState;
#endif /* HW_STM32F0XX_USART_H */
