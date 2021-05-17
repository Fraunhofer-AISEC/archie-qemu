/*
 * XMC4500 SoC
 *
 */


#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "hw/arm/xmc4500_soc.h"
#include "hw/qdev-properties.h"
#include "sysemu/sysemu.h"
#include "hw/misc/unimp.h"

static void xmc4500_soc_initfn(Object *obj)
{
	XMC4500State *s = XMC4500_SOC(obj);

	object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);
}

static void xmc4500_soc_realize(DeviceState *dev_soc, Error **errp)
{
	XMC4500State *s = XMC4500_SOC(dev_soc);
	//DeviceState *dev, *armv7m;
	DeviceState *armv7m;
	//SysBusDevice *busdev;

	MemoryRegion *system_memory = get_system_memory();
	MemoryRegion *sram = g_new(MemoryRegion, 1);
	MemoryRegion *flash = g_new(MemoryRegion, 1);
	MemoryRegion *flash_alias = g_new(MemoryRegion, 1);

	memory_region_init_rom(flash, OBJECT(dev_soc), "XMC4500.flash",
			FLASH_SIZE, &error_fatal);
	memory_region_init_alias(flash_alias, OBJECT(dev_soc), 
			"XMC4500.flash.alias", flash, 0, FLASH_SIZE);

	memory_region_add_subregion(system_memory, FLASH_BASE_ADDRESS, flash);
	memory_region_add_subregion(system_memory, 0, flash_alias);

	memory_region_init_ram(sram, NULL, "XMC4500.sram", SRAM_SIZE,
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
}

static Property xmc4500_soc_properties[] = {
	DEFINE_PROP_STRING("cpu-type", XMC4500State, cpu_type),
	DEFINE_PROP_END_OF_LIST(),
};

static void xmc4500_soc_class_init(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);

	dc->realize = xmc4500_soc_realize;
	device_class_set_props(dc, xmc4500_soc_properties);
}

static const TypeInfo xmc4500_soc_info = {
	.name		= TYPE_XMC4500_SOC,
	.parent		= TYPE_SYS_BUS_DEVICE,
	.instance_size	= sizeof(XMC4500State),
	.instance_init	= xmc4500_soc_initfn,
	.class_init	= xmc4500_soc_class_init,
};

static void xmc4500_soc_types(void)
{
	type_register_static(&xmc4500_soc_info);
}

type_init(xmc4500_soc_types)
