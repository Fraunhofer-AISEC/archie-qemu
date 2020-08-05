/*
 * Copyright (C) 2018, Emilio G. Cota <cota@braap.org>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>


#include "qemu/osdep.h"
#include "qemu-common.h"
#include <qemu/qemu-plugin.h>
#include <qemu/plugin.h>
#include "hw/core/cpu.h"
//#include "qemu/exec.h"
//#include "cpu.h"
//#include "exec/exec-all.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static void tb_exec_cb(unsigned int vcpu_index, void *userdata)
{
	g_autoptr(GString) out = g_string_new("");
	g_string_append(out, "Executed\n");
	plugin_flush_tb();
	g_string_append(out, "Flushing tb cache\n");
	qemu_plugin_outs(out->str);
}

/*
 * Empty TB translation callback.
 * This allows us to measure the overhead of injecting and then
 * removing empty instrumentation.
 */
static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "\n");
//    	qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_cb, QEMU_PLUGIN_CB_NO_REGS, NULL); 
//
	g_string_append_printf(out, "Virt1 value %8lx, hw addr1 %p\n", tb->vaddr, tb->haddr1);
	for(int i = 0; i < tb->n; i++)
	{
        	struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
//		g_string_append_printf(out, "This is the HW address      %lx\n",    *((long unsigned int *)(insn->haddr)));
		g_string_append_printf(out, "%8lx ", insn->vaddr);
		if (insn->vaddr == 0x80000dc)
		{
			g_string_append_printf(out, "%s\n", qemu_plugin_insn_disas( insn));
			qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_cb, QEMU_PLUGIN_CB_NO_REGS, NULL);
			CPUState *cpu = current_cpu;
			char tmp[2];
			int ret;
			ret = cpu_memory_rw_debug(cpu, 0x80000dc, tmp, 2, 0);
			if( ret == -1)
			{
				return;
			}
			tmp[1]++;
			cpu_memory_rw_debug(cpu, 0x80000dc, tmp, 2, 1);
//			*(((char *)tb->haddr1  ) + i*2) = (*((char *)tb->haddr1 + i*2) + 1);	
//			int cpu_memory_rw_debug(CPUState *cpu, target_ulong addr, void *ptr, target_ulong len, bool is_write)
		}
		g_string_append_printf(out, "%s\n", qemu_plugin_insn_disas( insn));
	}
	g_string_append_printf(out, "Virt2 value %8lx, hw addr2 %p\n", tb->vaddr2, tb->haddr2);
	qemu_plugin_outs(out->str);


}

static void vcpu_init_cb(qemu_plugin_id_t id, unsigned int vcpu_index)
{
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "CPU Initialised\n");
	qemu_plugin_outs(out->str);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    g_autoptr(GString) out = g_string_new("");
    g_string_printf(out, "QEMU Injection Plugin\nCurrent Target is %s\n", info->target_name);
    g_string_append_printf(out, "Current Version of QEMU is %i, Min Version is %i\n", info->version.cur, info->version.min );
    if(strcmp(info->target_name, "arm") < 0)
    {
	    g_string_append(out, "Arbort plugin, as this architecture is currently not supported!\n");
	    g_string_append_printf(out, "STRCMP value %i", strcmp(info->target_name, "arm"));
	    qemu_plugin_outs(out->str);
	    return -1;
    }
    qemu_plugin_outs(out->str);
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_vcpu_init_cb( id,  vcpu_init_cb);

    return 0;
}
