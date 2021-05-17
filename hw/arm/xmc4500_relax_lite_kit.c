/*
 * xmc4500 relax lite kit
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/qdev-properties.h"
#include "qemu/error-report.h"
#include "hw/arm/xmc4500_soc.h"
#include "hw/arm/boot.h"


/* Main SYSCLK frequency in HZ (4 - 168 MHZ)*/
#define SYSCLK_FRQ 168000000ULL

static void xmc4500relaxlitekit_init(MachineState *machine)
{
	DeviceState *dev;

	system_clock_scale = NANOSECONDS_PER_SECOND / SYSCLK_FRQ;

	dev = qdev_new(TYPE_XMC4500_SOC);
	qdev_prop_set_string(dev, "cpu-type", ARM_CPU_TYPE_NAME("cortex-m4"));
	sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

	armv7m_load_kernel(ARM_CPU(first_cpu), machine->kernel_filename, FLASH_SIZE);
}

static void xmc4500relaxlitekit_machine_init(MachineClass *mc)
{
	mc->desc = "XMC4500 Relax Lite Kit board (xmc4500)";
	mc->init = xmc4500relaxlitekit_init;
	mc->ignore_memory_transaction_failures = false;
}

DEFINE_MACHINE("xmc4500relaxlite", xmc4500relaxlitekit_machine_init)
