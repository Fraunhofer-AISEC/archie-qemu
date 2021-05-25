

#include "singlestep.h"
#include "tb_info_data_collection.h"
#include "tb_faulted_collection.h"

typedef struct tb_faulted_t tb_faulted_t;
typedef struct tb_faulted_t
{
	uint64_t trigger_address;
	GString * assembler;
	tb_faulted_t *next;
}tb_faulted_t;


tb_faulted_t *tb_faulted_list;

uint64_t *active_triggers;
int	max_triggers;
int	current_triggers;
int 	done_triggers;


/**
 * insert_faulted_assembly
 *
 * get needed list element and add assembly to it
 */
void insert_faulted_assembly(struct qemu_plugin_tb *tb, int trigger_address);

void tb_faulted_init(int number_faults)
{
	tb_faulted_list = NULL;
	active_triggers = malloc(sizeof(uint64_t)*number_faults);
	for(int i = 0; i < number_faults; i++)
	{
		*(active_triggers + i) = 0xdeadbeef;
	}
	max_triggers = number_faults;
	current_triggers = 0;
	done_triggers = 0;
	qemu_plugin_outs("[TBFaulted] Init done\n");
}

void tb_faulted_free(void)
{
	tb_faulted_t *item = NULL;
	while(tb_faulted_list != NULL)
	{
		item = tb_faulted_list;
		tb_faulted_list = tb_faulted_list->next;
		free(item);
	}
	if(active_triggers != NULL)
	{
		free(active_triggers);
	}
	max_triggers = 0;
	current_triggers = 0;
	done_triggers = 0;
}

void insert_faulted_assembly(struct qemu_plugin_tb *tb, int trigger_address)
{
	tb_faulted_t *item = tb_faulted_list;
	while(item != NULL)
	{
		if(item->trigger_address == trigger_address)
		{
			break;
		}
		item = item->next;
	}
	if(item == NULL)
	{
		qemu_plugin_outs("[TBFaulted]: Found no fault to be assembled!\n");
		return;
	}
	rem_singlestep_req();
	item->assembler = decode_assembler(tb);
}

void tb_faulted_register(uint64_t fault_address)
{
	if(max_triggers == current_triggers)
	{
		qemu_plugin_outs("[TBFaulted]: Registered tb faulted failed\n");
		return;
	}
	qemu_plugin_outs("[TBFaulted]: Registered tb faulted to be saved\n");
	add_singlestep_req();
	tb_faulted_t *item = malloc(sizeof(tb_faulted_t));
	item->trigger_address = fault_address;
	item->assembler = NULL;
	item->next = tb_faulted_list;
	tb_faulted_list = item;
	*(active_triggers + current_triggers ) = fault_address;
	current_triggers++;
}

void check_tb_faulted(struct qemu_plugin_tb *tb)
{
	if(done_triggers == current_triggers)
	{
		return;
	}
	size_t tb_size = calculate_bytesize_instructions(tb);
	for(int i = 0; i < current_triggers; i++)
	{
		if((*(active_triggers + i) >= tb->vaddr) && (*(active_triggers + i) <= tb->vaddr + tb_size) )
		{
			qemu_plugin_outs("[TBFaulted]: Found tb faulted to be saved\n");
			insert_faulted_assembly(tb, *(active_triggers + i));
			*(active_triggers + i) = 0xdeadbeaf;
			done_triggers++;
		}
	}
}

void dump_tb_faulted_data(void)
{
	if(tb_faulted_list == NULL)
	{
		qemu_plugin_outs("[TBFaulted]: Found no tb faulted list\n");
		return;
	}
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "$$$[TB Faulted]\n");
	plugin_write_to_data_pipe(out->str, out->len);
	tb_faulted_t *item = tb_faulted_list;
	while(item != NULL)
	{
		g_string_printf(out, "$$0x%lx | %s \n", item->trigger_address, item->assembler->str);
		plugin_write_to_data_pipe(out->str, out->len);
		item = item->next;
	}
}
