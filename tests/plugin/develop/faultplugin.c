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


#include "qemu/osdep.h"
#include "qemu-common.h"
#include <qemu/qemu-plugin.h>
#include <qemu/plugin.h>
#include "hw/core/cpu.h"


#define DEBUG_QEMU
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
	int address;
	int hitcounter;
} fault_trigger_t;

typedef struct
{
	int address;
	int type;
	int model;
	int lifetime;
	long long mask;
	fault_trigger_t trigger;
} fault_t;

typedef struct fault_list_t fault_list_t;
typedef struct fault_list_t
{
	fault_list_t *next;
	fault_t fault;
};


fifos_t * pipes;

fault_list_t *first_fault;

/*QEMU plugin install*/

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

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

int qemu_setup_config()
{
	
}

int add_fault(int fault_address, int fault_type, int fault_model, int fault_lifetime, long long fault_mask, int fault_trigger_address, int fault_trigger_hitcounter )
{
	fault_list_t *new_fault;
	new_fault = malloc(sizeof(fault_list_t));
	new_fault->next = NULL;
	new_fault->fault.address = fault_address;
	new_fault->fault.type = fault_type;
	new_fault->fault.model = fault_model;
	new_fault->fault.lifetime = fault_lifetime;
	new_fault->fault.mask = fault_mask;
	new_fault->fault.trigger.address = fault_trigger_address;
	new_fault->fault.trigger.hitcounter = fault_trigger_hitcounter;
	if(first_fault != NULL)
	{
		new_fault->next = first_fault;
	}
		first_fault = new_fault;
	return 0;
}

void delet_fault_queue()
{
	fault_list_t *del_item = NULL;
	while(first_fault != NULL)
	{
		del_item = first_fault;
		first_fault = first_fault->next;
		free(del_item);
	}
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, 
					const qemu_info_t *info,
					int argc, char **argv)
{
	pipes = NULL;
	first_fault = NULL;
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
	g_string_append(out, "Reached end of Initialisation, aborting now\n");
	qemu_plugin_outs(out->str);
	return -1;
}
