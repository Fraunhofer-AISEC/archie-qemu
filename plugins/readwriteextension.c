/**
 * QEMU Plugin read write extension code
 *
 * This is the code that allows the plugin to read and write
 * memory and registers and flush the tb cache. Also allows
 * to set QEMU into singlestep mode from Plugin.
 *
 * Based on plugin interface
 * 2017, Emilio G. Cota <cota@braap.org>
 * 2019, Linaro
 *
 * Copyright (C) 2021 Florian Hauschild <florian.hauschild@tum.de>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */



#include "qemu/osdep.h"
#include "qemu/plugin.h"
#include "hw/core/cpu.h"
#include "cpu.h"
#include "exec/exec-all.h"

void plugin_async_flush_tb(CPUState *cpu, run_on_cpu_data arg);
void plugin_async_flush_tb(CPUState *cpu, run_on_cpu_data arg)
{
	g_assert(cpu_in_exclusive_context(cpu));
	tb_flush(cpu);
}



int plugin_rw_memory_cpu(uint64_t address, uint8_t buffer[], size_t buf_size, char write)
{
	return cpu_memory_rw_debug( current_cpu, address, buffer, buf_size, write);
	//maybe cpu_physical_memory_rw(hwaddr addr, void *buf, hwaddr len, bool is_write)

}


void plugin_flush_tb(void)
{
//	g_assert(cpu_in_exclusive_context(current_cpu));
	async_safe_run_on_cpu(current_cpu, plugin_async_flush_tb, RUN_ON_CPU_NULL);
//	tb_flush(current_cpu);
}

static int plugin_read_register(CPUState *cpu, GByteArray *buf, int reg)
{
	CPUClass *cc = CPU_GET_CLASS(cpu);
	if (reg < cc->gdb_num_core_regs) {
		return cc->gdb_read_register(cpu, buf, reg);
	}

	return 0;
}

uint64_t read_reg(int reg)
{
	GByteArray *val = g_byte_array_new();
	uint64_t reg_ret = 0;
	int ret_bytes = plugin_read_register(current_cpu, val, reg);
	if(ret_bytes == 1)
	{
		reg_ret = val->data[0];
	}
	if(ret_bytes == 2)
	{
		reg_ret = *(uint16_t *) &(val->data[0]);
	}
	if(ret_bytes == 4)
	{
		reg_ret = *(uint32_t *) &(val->data[0]);
	}
	if(ret_bytes == 8)
	{
		reg_ret = *(uint64_t *) &(val->data[0]);
	}
	return reg_ret;
}

void write_reg(int reg, uint64_t val)
{
	//gdb_write_register(current_cpu, (uint8_t *) &val, reg);
	CPUState *cpu = current_cpu;
	CPUClass *cc = CPU_GET_CLASS(cpu);
	//CPUArchState *env = cpu->env_ptr;
	//GDBRegisterState *r;

	if (reg < cc->gdb_num_core_regs) {
		cc->gdb_write_register(cpu, (uint8_t *) &val, reg);
	}

	/*for (r = cpu->gdb_regs; r; r = r->next) {
		if (r->base_reg <= reg && reg < r->base_reg + r->num_regs) {
			return r->set_reg(env, mem_buf, reg - r->base_reg);
		}
	}*/
	return;
}

void plugin_single_step(int enable)
{
	static int orig_value; //preserve original value
	static int executed = 0;
	if(unlikely(executed == 0))
	{
		orig_value = singlestep; // save original value
		executed = 1; // now mark, that function was executed at least once
		g_autoptr(GString) out = g_string_new("");
		g_string_printf(out, "[SINGLESTEP_API]: %i\n", orig_value);
		qemu_log_mask(CPU_LOG_PLUGIN, "%s", out->str);
	}
	if(enable == 1)
	{
		singlestep = 1; // flag, that globally forces qemu into singlestep mode. setup in softmmu/vl.c
	}
	else
	{
		singlestep = orig_value;
		g_autoptr(GString) out = g_string_new("");
		g_string_printf(out, "[SINGLESTEP_API]: %i\n", singlestep);
		qemu_log_mask(CPU_LOG_PLUGIN, "%s", out->str);
	}
	// Force flush tb cache to bring singlestep into effect
	tb_flush(current_cpu);
}
