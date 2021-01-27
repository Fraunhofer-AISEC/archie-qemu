#include "fault_injection.h"
#include "faultplugin.h"

#include <qemu/qemu-plugin.h>
//#include <glib.h>

/**
 * inject_fault
 *
 * At this point the fault need to be injected. This is the function to select the right model and call injection function
 *
 * current: Struct address containing the fault information needed
 */
void inject_fault(fault_list_t * current)
{
	g_autoptr(GString) out = g_string_new("");
	if( current != NULL)
	{
		if(current->fault.type == FLASH)
		{
			qemu_plugin_outs("[Fault] Inject flash fault\n");
			inject_memory_fault( current);
			plugin_flush_tb();
			qemu_plugin_outs("Fulshed tb\n");
		}
		if(current->fault.type == SRAM)
		{
			qemu_plugin_outs("[Fault] Inject sram fault\n");
			inject_memory_fault( current);
			plugin_flush_tb();
			qemu_plugin_outs("Flushed tb\n");
		}
		if(current->fault.type == REGISTER)
		{
			qemu_plugin_outs("[Fault] Inject register fault\n");
			inject_register_fault( current);
			//TODO
		}
		//Remove fault trigger
		//*(fault_trigger_addresses + current->fault.trigger.trignum) = 0;
		invalidate_fault_trigger_address(current->fault.trigger.trignum);
		rem_singlestep_req();
		if(current->fault.lifetime != 0)
		{
				current->fault.trigger.trignum = register_live_faults_callback(current);
		}
	}
}

/**
 * reverse_fault
 *
 * Reverse the fault injected
 *
 * current: fault description 
 */
void reverse_fault(fault_list_t * current)
{
	g_autoptr(GString) out = g_string_new("");
	if(current != NULL)
	{
		if(current->fault.type == FLASH)
		{
			qemu_plugin_outs("[Fault] Reverse flash fault\n");
			process_reverse_fault(current->fault.address, current->fault.mask, current->fault.restoremask);
			plugin_flush_tb();
			qemu_plugin_outs("Flushed tb\n");
		}
		if(current->fault.type == SRAM)
		{
			qemu_plugin_outs("[Fault] Reverse flash fault\n");
			process_reverse_fault(current->fault.address, current->fault.mask, current->fault.restoremask);
			plugin_flush_tb();
			qemu_plugin_outs("Flushed tb\n");
		}
		if(current->fault.type == REGISTER)
		{
			qemu_plugin_outs("[Fault] Reverse register fault\n");
			reverse_register_fault( current);
		}
	}
	rem_singlestep_req();
}

/**
 * inject_register_fault
 *
 * Inject fault into registers. Reads the current string and determens the register attacked, loads it and performes the fault required
 */
void inject_register_fault(fault_list_t * current)
{
	g_autoptr(GString) out = g_string_new("");
	if(current->fault.address > 14)
	{
		qemu_plugin_outs("[ERROR] Register not valid\n");
		return;
	}
	uint32_t reg = read_arm_reg(current->fault.address);
	uint32_t mask = 0;
	for(int i = 0; i < 4; i++)
	{
		current->fault.restoremask[i] = (reg >> 8*i) & current->fault.mask[i];
		mask += (current->fault.mask[i] << 8*i);
	}
	g_string_printf(out," Changing registert %li from %08x", current->fault.address, reg);
	switch(current->fault.model)
	{
		case SET0:
			reg = reg & ~(mask);
			break;
		case SET1:
			reg = reg | mask;
			break;
		case TOGGLE:
			reg = reg ^ mask;
			break;
		default:
			break;
	}
	g_string_append_printf(out, " to %08x\n", reg);
	qemu_plugin_outs(out->str);
}

void reverse_register_fault(fault_list_t * current)
{
	g_autoptr(GString) out = g_string_new("");
	uint32_t reg = read_arm_reg(current->fault.address);

	g_string_printf(out, " Change register %li back from %08x", current->fault.address, reg);
	for(int i = 0; i < 4; i++)
	{
		reg = reg & ~((uint32_t)current->fault.mask[i] << 8*i); // clear manipulated bits
		reg = reg | ((uint32_t) current->fault.restoremask[i] << 8*i); // restore manipulated bits
	}
	g_string_printf(out, " to %08x\n", reg);
	qemu_plugin_outs(out->str);
}

/**
 * inject_memory_fault
 *
 * injects fault into memory regions
 * Reads current struct to determen the location, model, and mask of fault.
 * Then performes the fault injection
 *
 * current: Struct address containing the fault information
 */
void inject_memory_fault(fault_list_t * current)
{
	g_autoptr(GString) out = g_string_new("");
	switch(current->fault.model)
	{
		case SET0:
			g_string_append_printf(out, "Set 0 fault to Address %lx\n", current->fault.address);
			process_set0_memory(current->fault.address, current->fault.mask, current->fault.restoremask);
			break;
		case SET1:
			g_string_append_printf(out, "Set 1 fault to Address %lx\n", current->fault.address);
			process_set1_memory(current->fault.address, current->fault.mask, current->fault.restoremask);
			break;
		case TOGGLE:
			g_string_append_printf(out, "Toggle fault to Address %lx\n", current->fault.address);
			process_toggle_memory(current->fault.address, current->fault.mask, current->fault.restoremask);
			break;
		default:
			break;
	}
	qemu_plugin_outs(out->str);

}

/**
 * process_set1_memory
 *
 * Read memory, then apply set1 according to mask, then write memory back
 * 
 * address: baseaddress of lowest byte
 * mask: mask containing which bits need to be flipped to 1
 */
void process_set1_memory(uint64_t address, uint8_t  mask[], uint8_t restoremask[])
{
	uint8_t value[16];
	int ret;
	//ret = cpu_memory_rw_debug( cpu, address, value, 16, 0);
	ret = plugin_rw_memory_cpu( address, value, 16, 0);
	for(int i = 0; i < 16; i++)
	{
		restoremask[i] = value[i] & mask[15 - i]; //generate restoremaks
		value[i] = value[i] | mask[15 - i]; //inject fault
	}
	//ret += cpu_memory_rw_debug( cpu, address, value, 16, 1);
	ret += plugin_rw_memory_cpu( address, value, 16, 1);
	if (ret < 0)
	{
		qemu_plugin_outs("[ERROR]: Somthing went wrong in read/write to cpu in process_set1_memory\n");
	}
}

/**
 * process_reverse_fault
 *
 * Read memory, then apply inverse set0 according to mask, then write memory back
 *
 * address: baseaddress of fault
 * maks: location mask of bits set to 0 for reverse
 */
void process_reverse_fault(uint64_t address, uint8_t mask[], uint8_t restoremask[])
{
	uint8_t value[16];
	int ret;
	ret = plugin_rw_memory_cpu( address, value, 16, 0);
	for(int i = 0; i < 16; i++)
	{
		value[i] = value[i] & ~(mask[15 - i]); //clear value in mask position
		value[i] = value[i] | restoremask[i]; // insert restoremask to restore positions
	}
	ret += plugin_rw_memory_cpu( address, value, 16, 1);
	qemu_plugin_outs("[Fault]: Reverse fault!");
	if (ret < 0)
	{
		qemu_plugin_outs("[ERROR]: Somthing went wrong in read/write to cpu in process_reverse_fault\n");
	}
}

/**
 * process_set0_memory
 *
 * Read memory, then apply set0 according to mask, then write memory back
 *
 * address: baseaddress of fault
 * mask: location mask of bits set to 0 
 */
void process_set0_memory(uint64_t address, uint8_t  mask[], uint8_t restoremask[])
{
	uint8_t value[16];
	int ret;
	/*TODO Return validate*/
	//ret = cpu_memory_rw_debug( cpu, address, value, 16, 0);
	ret = plugin_rw_memory_cpu( address, value, 16, 0);
	for(int i = 0; i < 16; i++)
	{
		restoremask[i] = value[i] & mask[15 - i]; //generate restore mask
		value[i] = value[i] & ~(mask[15 - i]); //inject fault
	}
	//ret += cpu_memory_rw_debug( cpu, address, value, 16, 1);
	ret += plugin_rw_memory_cpu( address, value, 16, 1);
	if (ret < 0)
	{
		qemu_plugin_outs("[ERROR]: Somthing went wrong in read/write to cpu in process_set0_memory\n");
	}
}



/**
 * process_toggle_memory
 *
 * Read memory, then toggle bits to mask, then write memory back
 *
 * address: baseaddress of fault
 * mask: location mask of bits to be toggled
 */
void process_toggle_memory(uint64_t address, uint8_t  mask[], uint8_t restoremask[])
{
	uint8_t value[16];
	int ret;
	//ret = cpu_memory_rw_debug( cpu, address, value, 16, 0);
	ret = plugin_rw_memory_cpu( address , value, 16, 0);
	for(int i = 0; i < 16; i++)
	{
		restoremask[i] = value[i] & mask[15 - i]; //generate restore mask
		value[i] = value[i] ^ mask[15 - i]; //inject fault
	}
	//ret += cpu_memory_rw_debug( cpu, address - 1, value, 16, 1);
	ret += plugin_rw_memory_cpu( address, value, 16, 1);
	if (ret < 0)
	{
		qemu_plugin_outs("[ERROR]: Somthing went wrong in read/write to cpu in process_toggle_memory\n");
	}
}
