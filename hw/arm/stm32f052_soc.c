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

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "hw/arm/stm32f052_soc.h"
#include "hw/qdev-properties.h"
#include "sysemu/sysemu.h"
#include "hw/misc/unimp.h"

/* At the moment only Timer 2 to 5 are modelled */
/*static const uint32_t timer_addr[STM_NUM_TIMERS] = { 0x40000000, 0x40000400,
    0x40000800, 0x40000C00 };
static const uint32_t usart_addr[STM_NUM_USARTS] = { 0x40011000, 0x40004400,
    0x40004800, 0x40004C00, 0x40005000, 0x40011400 };
static const uint32_t adc_addr[STM_NUM_ADCS] = { 0x40012000, 0x40012100,
    0x40012200 };
static const uint32_t spi_addr[STM_NUM_SPIS] = { 0x40013000, 0x40003800,
    0x40003C00 };

static const int timer_irq[STM_NUM_TIMERS] = {28, 29, 30, 50};
static const int usart_irq[STM_NUM_USARTS] = {37, 38, 39, 52, 53, 71};
#define ADC_IRQ 18
static const int spi_irq[STM_NUM_SPIS] = {35, 36, 51};
*/
static void stm32f052_soc_initfn(Object *obj)
{
    STM32F052State *s = STM32F052_SOC(obj);
//    int i;

    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);

//    object_initialize_child(obj, "syscfg", &s->syscfg, TYPE_STM32F2XX_SYSCFG);

  /*  for (i = 0; i < STM_NUM_USARTS; i++) {
        object_initialize_child(obj, "usart[*]", &s->usart[i],
                                TYPE_STM32F2XX_USART);
    }

    for (i = 0; i < STM_NUM_TIMERS; i++) {
        object_initialize_child(obj, "timer[*]", &s->timer[i],
                                TYPE_STM32F2XX_TIMER);
    }

    s->adc_irqs = OR_IRQ(object_new(TYPE_OR_IRQ));

    for (i = 0; i < STM_NUM_ADCS; i++) {
        object_initialize_child(obj, "adc[*]", &s->adc[i], TYPE_STM32F2XX_ADC);
    }

    for (i = 0; i < STM_NUM_SPIS; i++) {
        object_initialize_child(obj, "spi[*]", &s->spi[i], TYPE_STM32F2XX_SPI);
    }*/
}

static void stm32f052_soc_realize(DeviceState *dev_soc, Error **errp)
{
    STM32F052State *s = STM32F052_SOC(dev_soc);
    DeviceState *armv7m; 
//   DeviceState *dev, *armv7m;
//    SysBusDevice *busdev;
//    int i;

    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *flash_alias = g_new(MemoryRegion, 1);

    memory_region_init_rom(flash, OBJECT(dev_soc), "STM32F052.flash",
                           FLASH_SIZE, &error_fatal);
    memory_region_init_alias(flash_alias, OBJECT(dev_soc),
                             "STM32F052.flash.alias", flash, 0, FLASH_SIZE);

    memory_region_add_subregion(system_memory, FLASH_BASE_ADDRESS, flash);
    memory_region_add_subregion(system_memory, 0, flash_alias);

    memory_region_init_ram(sram, NULL, "STM32F052.sram", SRAM_SIZE,
                           &error_fatal);
    memory_region_add_subregion(system_memory, SRAM_BASE_ADDRESS, sram);

    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", 32);
    qdev_prop_set_string(armv7m, "cpu-type", s->cpu_type);
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(get_system_memory()), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }

    /* System configuration controller */
/*    dev = DEVICE(&s->syscfg);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->syscfg), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, 0x40010000);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, 31));
*/
    /* Attach UART (uses USART registers) and USART controllers */
 /*   for (i = 0; i < STM_NUM_USARTS; i++) {
        dev = DEVICE(&(s->usart[i]));
        qdev_prop_set_chr(dev, "chardev", serial_hd(i));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->usart[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, usart_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, usart_irq[i]));
    }
*/
    /* Timer 2 to 5 */
/*    for (i = 0; i < STM_NUM_TIMERS; i++) {
        dev = DEVICE(&(s->timer[i]));
        qdev_prop_set_uint64(dev, "clock-frequency", 1000000000);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->timer[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, timer_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, timer_irq[i]));
    }
*/
    /* ADC 1 to 3 */
/*    object_property_set_int(OBJECT(s->adc_irqs), "num-lines", STM_NUM_ADCS,
                            &error_abort);
    if (!qdev_realize(DEVICE(s->adc_irqs), NULL, errp)) {
        return;
    }
    qdev_connect_gpio_out(DEVICE(s->adc_irqs), 0,
                          qdev_get_gpio_in(armv7m, ADC_IRQ));

    for (i = 0; i < STM_NUM_ADCS; i++) {
        dev = DEVICE(&(s->adc[i]));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->adc[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, adc_addr[i]);
        sysbus_connect_irq(busdev, 0,
                           qdev_get_gpio_in(DEVICE(s->adc_irqs), i));
    }
*/
    /* SPI 1 and 2 */
/*    for (i = 0; i < STM_NUM_SPIS; i++) {
        dev = DEVICE(&(s->spi[i]));
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->spi[i]), errp)) {
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, spi_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, spi_irq[i]));
    }*/

    create_unimplemented_device("SYSCFG COMP",	0x40010000 , 0x400);
    create_unimplemented_device("EXTI",		0x40010400 , 0x400);
    create_unimplemented_device("USART[6]",	0x40011400 , 0x400);
    create_unimplemented_device("USART[7]",	0x40011800 , 0x400);
    create_unimplemented_device("USART[8]",	0x40011C00 , 0x400);
    create_unimplemented_device("ADC",		0x40012400 , 0x400);
    create_unimplemented_device("TIM1",		0x40012C00 , 0x400);
    create_unimplemented_device("SPI/I2S1",	0x40013000 , 0x400);
    create_unimplemented_device("USART[1]",	0x40013800 , 0x400);
    create_unimplemented_device("TIM15",	0x40014000 , 0x400);
    create_unimplemented_device("TIM16",	0x40014400 , 0x400);
    create_unimplemented_device("TIM17",	0x40014800 , 0x400);
    create_unimplemented_device("DBGMCU",	0x40015800 , 0x400);
    create_unimplemented_device("DMA1",		0x40020000 , 0x400);
    create_unimplemented_device("DMA2",		0x40020400 , 0x400);
    create_unimplemented_device("RCC",		0x40021000 , 0x400);
    create_unimplemented_device("FLASH",	0x40022000 , 0x400);
    create_unimplemented_device("CRC",		0x40023000 , 0x400);
    create_unimplemented_device("TSC",		0x40024000 , 0x400);
    create_unimplemented_device("GPIOA",	0x48000000 , 0x400);
    create_unimplemented_device("GPIOB",	0x48000400 , 0x400);
    create_unimplemented_device("GPIOC",	0x48000800 , 0x400);
    create_unimplemented_device("GPIOD",	0x48000C00 , 0x400);
    create_unimplemented_device("GPIOE",	0x48001000 , 0x400);
    create_unimplemented_device("GPIOF",	0x48001400 , 0x400);
}

static Property stm32f052_soc_properties[] = {
    DEFINE_PROP_STRING("cpu-type", STM32F052State, cpu_type),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32f052_soc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32f052_soc_realize;
    device_class_set_props(dc, stm32f052_soc_properties);
}

static const TypeInfo stm32f052_soc_info = {
    .name          = TYPE_STM32F052_SOC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F052State),
    .instance_init = stm32f052_soc_initfn,
    .class_init    = stm32f052_soc_class_init,
};

static void stm32f052_soc_types(void)
{
    type_register_static(&stm32f052_soc_info);
}

type_init(stm32f052_soc_types)
