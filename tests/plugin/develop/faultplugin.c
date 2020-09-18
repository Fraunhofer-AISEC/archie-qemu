/*
 * Copyright (C) 2020, Florian Hauschild <florian.hauschild94@gmail.com>
 */


#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>
#include <sys/types.h>

#include "qemu/osdep.h"
#include "qemu-common.h"
#include <qemu/qemu-plugin.h>
#include <qemu/plugin.h>
#include "hw/core/cpu.h"


//DEBUG
#include <errno.h>
#include <string.h>

//#define DEBUG_QEMU
#ifndef DEBUG_QEMU
#define FIFO_READ O_RDONLY
#define FIFO_WRITE O_WRONLY
#else
#define FIFO_READ O_RDONLY | O_NONBLOCK
#define FIFO_WRITE O_WRONLY | O_NONBLOCK
#endif


enum{ SRAM, FLASH, REGISTER};
enum{ SET0, SET1, TOGGLE};

typedef struct
{
	int control;
	int config;
	int data;
} fifos_t;

typedef struct
{
	uint64_t address; //uint64_t?
	uint64_t hitcounter;
} fault_trigger_t;

typedef struct
{
	uint64_t address; //uint64_t?
	uint64_t type; //Typedef enum?
	uint64_t model;
	uint64_t lifetime;
	uint8_t mask[16]; // uint8_t array?
	fault_trigger_t trigger;
} fault_t;

typedef struct fault_list_t fault_list_t;
typedef struct fault_list_t
{
	fault_list_t *next;
	fault_t fault;
} fault_list_t;


/* Global data structures */

fifos_t * pipes;

fault_list_t *first_fault;

uint64_t *	fault_trigger_addresses;
uint64_t *	fault_addresses;
int	fault_number;
int 	first_tb;

/*QEMU plugin Version control*/

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;


/**
 *
 * parse_args
 *
 * Read in command line parameters. These are the Control, config and Data pipe paths.
 * They will be opend here. Commands are send over the Control pipe.
 * Configuration for faults is send over the config pipe
 * Data is send from this module to the outside over the data pipe
 *
 * argv: contains the different path strings
 * argc: number of strings
 *
 * return: Return -1 if somthing went wrong
 *
 * */

int parse_args(int argc, char **argv, GString *out)
{
	g_string_append_printf(out, "Starting argparsing\n");
	if(argc != 3)
	{
		g_string_append_printf(out, "Not the right ammount of arguments! %i\n", argc);
		return -1;
	}
	for(int i = 0; i < argc; i++)
	{
		int j = 0;
		char *s = NULL;
		/*Find end of String of file descriptor*/
		while( *(*(argv+i)+j) != 0 )
		{
			j++;
		}
		/*Copy string of fifo*/
		s = malloc(sizeof(char) * j);
		for(int k = 0; k <= j; k++)
		{
			*(s + k) = *(*(argv+i) + k);
		}
		g_string_append_printf(out, "FIFO %i path is %s\n", i, s);
		g_string_append_printf(out, "Open %s\n", s);
		switch(i)
		{
			case 0:
				pipes->control = open(s, FIFO_READ);
				break;
			case 1:
				pipes->config = open(s, FIFO_READ);
				break;
			case 2:
				pipes->data = open(s, FIFO_WRITE);
		}
		g_string_append_printf(out, "done\n");
		/*free string*/
		free(s);
	}
	return 0;
}


uint64_t char_to_uint64(char *c, int size_c)
{
	g_autoptr(GString) out = g_string_new("");
	uint64_t tmp = 0;
	int i = 0;
	g_string_printf(out, "This is the conversion function: ");
	for(i = 0; i < size_c; i++)
	{
		g_string_append_printf(out, " 0x%x",(char) *(c + i));
		tmp = tmp << 8;
		tmp += 0xff & (char) *(c + i);
	}
	g_string_append(out, "\n");
	qemu_plugin_outs(out->str);
	return tmp;
}


/**
 *
 * qemu_setup_config
 *
 * This function reads the config from the config pipe. It will only read one fault conviguration.
 * If multiple fault should be used, call this function multiple times
 */

int qemu_setup_config()
{
	g_autoptr(GString) out = g_string_new("");
	uint64_t fault_address = 0;
	uint64_t fault_type = 0;
	uint64_t fault_model = 0;
	uint64_t fault_lifetime = 0;
	uint8_t fault_mask[16];
	uint64_t fault_trigger_address = 0;
	uint64_t fault_trigger_hitcounter = 0;
	uint8_t buf[16];
	uint64_t target_len = 8;
	uint64_t tmp = 0xffffffffffffffff;
	g_string_printf(out, "Start redout of FIFO\n");
	for(int i = 0; i < 7; i++)
	{
		g_string_append_printf(out, "Parameter %i\n", i);
		//qemu_plugin_outs(out->str);
		ssize_t readout = 0;
		ssize_t read_bytes = 0;
		while(target_len > read_bytes)
		{
			readout = read(pipes->config, buf + readout, target_len - readout );
			g_string_append_printf(out, "DEBUG: readout %li, target_len %li \n", readout, target_len);
			if(readout == -1)
			{
				g_string_append_printf(out, "DEBUG: Value is negativ, Somthing happend in read: %s\n", strerror(errno));
				g_string_append_printf(out, "DEBUG: File descriptor is : %i\n", pipes->config);
				//qemu_plugin_outs(out->str);
				//g_string_printf(out, "  \n");
			}
			else
			{
				read_bytes += readout;
				readout = 0;
			}
		}
		g_string_append_printf(out, "done readout of pipe\n");
		//qemu_plugin_outs(out->str);
		switch(i)
		{
			case 0:
				fault_address = char_to_uint64(buf, 8);
				target_len = 8;
				g_string_append_printf(out, "fault address: 0x%lx\n", fault_address);
				break;
			case 1:
				fault_type = char_to_uint64(buf, 8);
				target_len = 8;
				g_string_append_printf(out, "fault address: 0x%lx\n", fault_type);
				break;
			case 2:
				fault_model = char_to_uint64(buf, target_len);
				target_len = 8;
				g_string_append_printf(out, "fault address: 0x%lx\n", fault_model);
				break;
			case 3:
				fault_lifetime = char_to_uint64(buf, target_len);
				target_len = 16;
				g_string_append_printf(out, "fault address: 0x%lx\n", fault_lifetime);
				break;
			case 4:
				//fault_mask = buf[15] << 15*8 | buf[14] << 14*8 | buf[13] << 13*8 | buf[12] << 12*8 | buf[11] << 11*8 | buf[10] << 10*8 | buf[9] << 9*8 | buf[8] << 8*8 | buf[7] << 7*8 | buf[6] << 6*8 | buf[5] << 5*8 | buf[4] << 4*8 |buf[3] << 3*8 | buf[2] << 2*8 | buf[1] << 1*8 | buf[0] << 0*8;
				g_string_append(out, "fault mask: ");
				for(int j = 0; j < 16; j++)
				{
					fault_mask[j] = buf[j];
					g_string_append_printf(out, " 0x%x", fault_mask[j]);
				}
				g_string_append(out, "\n");
				target_len = 8;
				break;
			case 5:
				fault_trigger_address = char_to_uint64(buf, target_len);
				target_len = 8;
				g_string_append_printf(out, "fault address: 0x%lx\n", fault_trigger_address);
				break;
			case 6:
				fault_trigger_hitcounter = char_to_uint64(buf, target_len);
				target_len = 8;
				g_string_append_printf(out, "fault address: 0x%lx\n", fault_trigger_hitcounter);
				break;
		}
	}
	g_string_append(out, "Fault pipe read done\n");
	qemu_plugin_outs(out->str);
	return add_fault(fault_address, fault_type, fault_model, fault_lifetime, fault_mask, fault_trigger_address, fault_trigger_hitcounter);
}


/**
 * add fault
 *
 * This function appends one fault to the linked list. 
 *
 * return -1 if fault
 */
int add_fault(uint64_t fault_address, uint64_t fault_type, uint64_t fault_model, uint64_t fault_lifetime, uint8_t fault_mask[16], uint64_t fault_trigger_address, uint64_t fault_trigger_hitcounter )
{
	fault_list_t *new_fault;
	new_fault = malloc(sizeof(fault_list_t));
	if( new_fault == NULL)
	{
		return -1;
	}
	new_fault->next = NULL;
	new_fault->fault.address = fault_address;
	new_fault->fault.type = fault_type;
	new_fault->fault.model = fault_model;
	new_fault->fault.lifetime = fault_lifetime;
	//new_fault->fault.mask = fault_mask;
	new_fault->fault.trigger.address = fault_trigger_address;
	new_fault->fault.trigger.hitcounter = fault_trigger_hitcounter;
	for(int i = 0; i < 16; i++)
	{
		new_fault->fault.mask[i] = fault_mask[i];
	}
	if(first_fault != NULL)
	{
		new_fault->next = first_fault;
	}
	first_fault = new_fault;
	return 0;
}

/**
 *
 * delete_fault_queue
 *
 * This function removes faults from linked list
 *
 */
void delete_fault_queue()
{
	fault_list_t *del_item = NULL;
	while(first_fault != NULL)
	{
		del_item = first_fault;
		first_fault = first_fault->next;
		free(del_item);
	}
}

/**
 * return_next
 *
 * function to return next pointer.
 * This is to be able to change the current link list if desired
 */
fault_list_t * return_next(fault_list_t * current)
{
	return current->next;
}

/**
 * get_fault_trigger_address
 *
 * function to return the fault address. 
 * This is to be able to change the current data structure if needed
 */
uint64_t get_fault_trigger_address(fault_list_t * current)
{
	return current->fault.trigger.address;
}

fault_list_t * get_fault_struct_by_trigger(uint64_t fault_trigger_address)
{
	fault_list_t * current = first_fault;
	while(current != NULL)
	{
		if(current->fault.trigger.address == fault_trigger_address)
		{
			return current;
		}
		current = current->next;
	}
	return NULL;
}


/**
 * register_fault_address
 *
 * This function will fill the global fault trigger address array and fault address array
 */
int register_fault_trigger_addresses()
{
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "Calculate number of faults\n");
	/*Select first element of list*/
	fault_list_t * current = first_fault;
	int i = 0;
	/*traverse list*/
	while(current != NULL)
	{
		i++;
		current = return_next(current);
	}
	g_string_append_printf(out, "number_of_faults %i\n",i);
	if(i == 0)
	{
		g_string_append(out, "No fault found!\n");
		qemu_plugin_outs(out->str);
		return -1;
	}
	/* Reset back to firs element*/
	current = first_fault;
	fault_number = i;
	g_string_append_printf(out, "Fault number %i\n", fault_number);
	/* Reserve Memory vor "Vector"*/
	fault_trigger_addresses = malloc(sizeof(uint64_t) * fault_number);
	fault_addresses = malloc(sizeof(uint64_t) * fault_number);
	if(fault_trigger_addresses == NULL || fault_addresses == NULL)
	{
		g_string_append_printf(out, "malloc failed here in registerfaulttrigger\n");
		qemu_plugin_outs(out->str);
		return -1;
	}
	g_string_append(out, "Start registering faults\n");
	g_string_append_printf(out, "fault trigger addresses: %p\n", fault_trigger_addresses);
	g_string_append_printf(out, "fault addresses: %p\n", fault_addresses);
	for(int j = 0; j < i; j++)
	{
		/* Fill Vector with value*/
		*(fault_trigger_addresses + j) = get_fault_trigger_address(current);
		*(fault_addresses + j) = 0;	
		g_string_append_printf(out, "fault trigger addresses: %p\n", fault_trigger_addresses+j);
		g_string_append_printf(out, "fault addresses: %p\n", fault_addresses+j);
		current = return_next(current);	
	}
	qemu_plugin_outs(out->str);
	return 0;	
}

/**
 * delete_fault_trigger_address
 */
void delete_fault_trigger_addresses()
{
	free(fault_trigger_addresses);
}

/**
 * process_set1_memory
 *
 * Read memory, then apply set1 according to mask, then write memory back
 */
void process_set1_memory(uint64_t address, uint8_t  mask[])
{
	uint8_t value[16];
	CPUState *cpu = current_cpu;
	int ret;
	ret = cpu_memory_rw_debug( cpu, address, value, 16, 0);
	for(int i = 0; i < 16; i++)
	{
		value[i] = value[i] | mask[i];
	}
	ret = cpu_memory_rw_debug( cpu, address, value, 16, 1);
}

/**
 * process_set0_memory
 *
 * Read memory, then apply set0 according to mask, then write memory back
 */
void process_set0_memory(uint64_t address, uint8_t  mask[])
{
	uint8_t value[16];
	CPUState *cpu = current_cpu;
	int ret;
	ret = cpu_memory_rw_debug( cpu, address, value, 16, 0);
	for(int i = 0; i < 16; i++)
	{
		value[i] = value[i] & ~(mask[i]);
	}
	ret = cpu_memory_rw_debug( cpu, address, value, 16, 1);
}

/**
 * process_toggle_memory
 *
 * Read memory, then toggle bits to mask, then write memory back
 */
void process_toggle_memory(uint64_t address, uint8_t  mask[])
{
	uint8_t value[16];
	CPUState *cpu = current_cpu;
	int ret;
	ret = cpu_memory_rw_debug( cpu, address, value, 16, 0);
	for(int i = 0; i < 16; i++)
	{
		value[i] = value[i] ^ mask[i];
	}
	ret = cpu_memory_rw_debug( cpu, address, value, 16, 1);
}
void inject_fault(fault_list_t * current)
{
	if( current != NULL)
	{
		if(current->fault.type = FLASH)
		{
			switch(current->fault.model)
			{
				case SET0:
					break;
				default:
					break;
			}
		}
	}
}



/**
 * handle_first_tb_fault_insertion
 *
 * This function is called in the first used tb block
 */
void handle_first_tb_fault_insertion()
{

	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "Look into if we need to insert a fault!\n");
	fault_list_t * current = first_fault;
	qemu_plugin_outs(out->str);
	while(current != NULL)
	{
		if(current->fault.trigger.hitcounter == 0 && current->fault.type == FLASH )
		{
			switch(current->fault.model)
			{
				case SET0:
					g_string_append_printf(out, "Set 0 fault to Address %lx\n", current->fault.address);
					process_set0_memory(current->fault.address, current->fault.mask);
					break;
				case SET1:
					g_string_append_printf(out, "Set 1 fault to Address %lx\n", current->fault.address);
					process_set1_memory(current->fault.address, current->fault.mask);
					break;
				case TOGGLE:
					g_string_append_printf(out, "Set toggle fault to Address %lx\n", current->fault.address);
	qemu_plugin_outs(out->str);
					process_toggle_memory(current->fault.address, current->fault.mask);
					break;
				default:
					break;
			}
		}
		current = return_next( current);
	}
	qemu_plugin_outs(out->str);

}


/*
 * calculate_bytesize_instructions
 *
 * Function to calculate size of TB. This is currently done 
 * by simple multiply on the assumption of thumb2 instructions
 */
size_t calculate_bytesize_instructions(struct qemu_plugin_tb *tb)
{
	return (size_t) (tb->n * 2);
}

void evaluate_trigger(int trigger_address_number)
{
	
	/*Get fault description*/
	fault_list_t *current = get_fault_struct_by_trigger((uint64_t) *(fault_trigger_addresses + trigger_address_number));
	current->fault.trigger.hitcounter = current->fault.trigger.hitcounter - 1;
	if(current->fault.trigger.hitcounter == 0)
	{
		/* Trigger met. Start injection */
		/* Remove trigger condition from internal struct*/
		*(fault_trigger_addresses + trigger_address_number) = 0;
		
	}
}

void handle_tb_translate_event(struct qemu_plugin_tb *tb)
{
	fault_list_t * current = first_fault;
	size_t tb_size = calculate_bytesize_instructions(tb);
	/**Verify, that no trigger is called*/
	for( int i = 0; i < fault_number; i++)
	{
		if((tb->vaddr < *(fault_trigger_addresses + i))&&((tb->vaddr + tb_size) > *(fault_trigger_addresses+i)))
		{
			evaluate_trigger( *(fault_trigger_addresses + i));
		}
	}
}

/**
 * vcpu_translateblock_translation_event
 *
 * main entry point for tb translation event
 */
static void vcpu_translateblock_translation_event(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "\n");

	g_string_append_printf(out, "Virt1 value: %8lx\n", tb->vaddr);

	qemu_plugin_outs(out->str);	
	if(first_tb != 0)
	{
			g_string_append_printf(out, "So we do Stuff here\n\n");
	}
	else
	{
		g_string_append_printf(out, "This is the first time the tb is translated\n");
		first_tb = 1;
		qemu_plugin_outs(out->str);
		g_string_printf(out, " ");
		handle_first_tb_fault_insertion();
	}
	qemu_plugin_outs(out->str);	
}

/**
 *
 * qemu_plugin_install
 *
 * This is the first called function. 
 *
 */
QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, 
		const qemu_info_t *info,
		int argc, char **argv)
{
	pipes = NULL;
	first_fault = NULL;
	fault_number = 0;
	fault_trigger_addresses = NULL;
	fault_addresses = NULL;
	first_tb = 0;
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "QEMU INjection Plugin\n Current Target is %s\n", info->target_name);
	g_string_append_printf(out, "Current Version of QEMU Plugin is %i, Min Version is %i\n", info->version.cur, info->version.min);
	if(strcmp(info->target_name, "arm") < 0)
	{
		g_string_append(out, "Arbort plugin, as this architecture is currently not supported!\n");
		qemu_plugin_outs(out->str);
		return -1;
	}
	pipes = malloc(sizeof(fifos_t));
	if(pipes == NULL)
	{
		goto ABBORT;
	}
	pipes->control = NULL;
	pipes->config = NULL;
	pipes->data = NULL;
	/*Start Argparsing and open up the Fifos*/
	if(parse_args(argc, argv, out) != 0)
	{
		g_string_append_printf(out, "Initialisation of FIFO failed!\n");
		qemu_plugin_outs(out->str);
		return -1;
	}
	g_string_append_printf(out, "Initialisation of FIFO Succeded!\n");
	g_string_append_printf(out, "Readout config FIFO\n");
	qemu_plugin_outs(out->str);
	g_string_printf(out, " ");
	if( qemu_setup_config() < 0)
	{
		goto ABBORT;
	}
	g_string_append_printf(out, "Linked list entry address: %p\n", first_fault);	

	g_string_append_printf(out, "Register fault trigger addresses\n");
	qemu_plugin_outs(out->str);
	g_string_printf(out, " ");
	if(register_fault_trigger_addresses() < 0 )
	{
		goto ABBORT;
	}
	g_string_append_printf(out, "Number of triggers: %i\n", fault_number);
	g_string_append(out, "Register VCPU tb trans callback\n");
	//qemu_plugin_register_vcpu_tb_trans_cb( id, vcpu_translateblock_translation_event);

	g_string_append_printf(out, "Reached end of Initialisation, starting guest now\n");
	qemu_plugin_outs(out->str);
	return 0;
ABBORT:
	delete_fault_trigger_addresses();
	delete_fault_queue();
	g_string_append(out, "Somthing went wrong. Abborting now!\n");
	qemu_plugin_outs(out->str);
	return -1;
}
