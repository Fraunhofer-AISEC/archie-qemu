
#include "tb_exec_data_collection.h"
#include "faultplugin.h"
#include <stdint.h>
#include <stdlib.h>

tb_exec_order_t *tb_exec_order_list;

uint64_t num_exec_order;

void tb_exec_order_init()
{
	// List of execution order of tbs.
	tb_exec_order_list = NULL;
	num_exec_order = 0;
}


/**
 * tb_exec_order_free()
 *
 * free linked list of tb_exec_order_t elements. It does not free the tb_info_t inside.
 * These must be freed seperatly with tb_info_free()
 */
void tb_exec_order_free()
{
	tb_exec_order_t *item;
	while(tb_exec_order_list != NULL)
	{
		item = tb_exec_order_list;
		tb_exec_order_list = tb_exec_order_list->prev;
		free(item);
	}
}


/**
 * plugin_dump_tb_exec_order
 *
 * Print the order of translation blocks executed. Also provide a counter number, that it can be later resorted in python
 */
void plugin_dump_tb_exec_order()
{
	uint64_t i = 0;
	if(tb_exec_order_list == NULL)
	{
		return;
	}
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "$$$[TB Exec]:\n");
	plugin_write_to_data_pipe(out->str, out->len);
	tb_exec_order_t *item =  tb_exec_order_list;

	while(item->prev != NULL)
	{
		i++;
		item = item->prev;
	}
	i++;
	if(i != num_exec_order)
	{
		qemu_plugin_outs("[WARNING]: i und numexec sind nicht gleich !\n");
	}
	i = 0;
	while(item != NULL)
	{
		g_string_printf(out, "$$ 0x%lx | %li \n", item->tb_info->base_address, i);
		plugin_write_to_data_pipe(out->str, out->len);
		item = item->next;
		i++;
	}
}

/**
 * tb_exec_data_event
 * 
 * Function to collect the exec data about translation blocks
 *
 * vcpu_index: current index of cpu the callback was triggered from
 * vcurrent: pointer to tb_info struct of the current tb
 */
void tb_exec_data_event(unsigned int vcpu_index, void *vcurrent)
{
	tb_info_t *tb_info = vcurrent;
	if(tb_info != NULL)
	{
		tb_info->num_of_exec++;
	}
	tb_exec_order_t *last = malloc(sizeof(tb_exec_order_t));
	last->tb_info = tb_info;
	last->next = NULL;
	last->prev = tb_exec_order_list;
	if(tb_exec_order_list != NULL)
	{
		tb_exec_order_list->next = last;
	}
	tb_exec_order_list = last;
	num_exec_order++;
	//
	//DEBUG	
	//	g_autoptr(GString) out = g_string_new("");
	//	g_string_printf(out, "[TB_exec]: ID: %x, Execs %i\n", tb_info->base_address, tb_info->num_of_exec);
	//	qemu_plugin_outs(out->str);
}
