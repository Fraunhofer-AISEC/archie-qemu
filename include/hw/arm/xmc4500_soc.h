/*
 * XMC4500
 *
 */

#ifndef HW_ARM_XMC4500_SOC_H
#define HW_ARM_XMC4500_SOC_H

#include "hw/arm/armv7m.h"

#define TYPE_XMC4500_SOC "xmc4500-soc"
#define XMC4500_SOC(obj) \
	OBJECT_CHECK(XMC4500State, (obj), TYPE_XMC4500_SOC)

#define FLASH_BASE_ADDRESS 0x08000000
#define FLASH_SIZE (768 *1024)
#define SRAM_BASE_ADDRESS 0x10000000
#define SRAM_SIZE (160 * 1024)

typedef struct XMC4500State {
	/*< private >*/
	SysBusDevice parent_obj;
	/*< public >*/

	char *cpu_type;

	ARMv7MState armv7m;

	//qemu_or_irq *adc_irqs;
}XMC4500State;
#endif
