#include "singlestep.h"

#include <qemu/qemu-plugin.h>
#include <glib.h>


volatile uint64_t req_singlestep = 0;

void init_singlestep_req()
{
	req_singlestep = 0;
}

void check_singlestep()
{
	if(req_singlestep == 0)
	{
		plugin_single_step(0);
	}
	else
	{
		plugin_single_step(1);
	}
	plugin_flush_tb();
}

void add_singlestep_req()
{
	g_autoptr(GString) out = g_string_new("");
	qemu_plugin_outs("[SINGLESTEP]: increase reqest\n");
	req_singlestep++;
	g_string_printf(out, "[SINGLESTEP]: requests %li\n", req_singlestep);
	qemu_plugin_outs(out->str);
	check_singlestep();
}

void rem_singlestep_req()
{
	if(req_singlestep != 0)
	{
		g_autoptr(GString) out = g_string_new("");
		qemu_plugin_outs("[SINGLESTEP]: decrease reqest\n");
		req_singlestep--;
		g_string_printf(out, "[SINGLESTEP]: requests %li\n", req_singlestep);
		qemu_plugin_outs(out->str);
		check_singlestep();
	}
}
