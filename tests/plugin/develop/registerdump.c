/*
 *   Copyright 2021 Florian Andreas Hauschild
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 *   This file contains all functions needed to collect register data and send
 *   it over the data pipe
 */
#include "registerdump.h"


typedef struct registerdump_t registerdump_t;
typedef struct registerdump_t
{
	uint64_t pc;
	uint64_t tbcount;
	uint32_t regs[17];
	registerdump_t *next;
} registerdump_t;

registerdump_t *first_registerdump;
int arch;

/**
 * readout_arm_registers
 *
 * readout arm registers from QEMU.
 *
 * @params current the current reigsterdump_t struct. It fills the regs part
 */
void readout_arm_registers(registerdump_t * current);

/**
 * read_arm_registers
 *
 * write registers to data pipe
 */
void read_arm_registers(void);

void init_register_module(int architecture)
{
	first_registerdump = NULL;
	arch = architecture; 
}

void delete_register_module(void)
{
	registerdump_t* current;
	while(first_registerdump != NULL)
	{
		current = first_registerdump;
		first_registerdump = first_registerdump->next;
		free(current);
	}
}

int add_new_registerdump(uint64_t tbcount)
{
	registerdump_t* current = malloc(sizeof(registerdump_t));
	current->next = first_registerdump;
	if(arch == ARM)
	{
		readout_arm_registers( current);
		current->pc = current->regs[15];
	}
	current->tbcount = tbcount;
	first_registerdump = current;
	return 0;
}

void readout_arm_registers(registerdump_t * current)
{
	// read r0 - r15
	for(int i = 0; i < 16; i++)
	{
		current->regs[i] = read_arm_reg(i); 
	}
	// read XPSR
	current->regs[16] = read_arm_reg(25);
}


void read_register_module(void)
{
	if(arch == ARM)
	{
		qemu_plugin_outs("[DEBUG]: start reading registerdumps\n");
		read_arm_registers();
		return;
	}
	qemu_plugin_outs("[ERROR]: [CRITICAL]: Unknown Architecture for register module");
}

void read_arm_registers(void)
{
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "$$$[Arm Registers]\n");
	plugin_write_to_data_pipe(out->str, out->len);
	registerdump_t* current = first_registerdump;
	while(current != NULL)
	{
		g_string_printf(out, "$$ %li | %li ", current->pc, current->tbcount);
		for(int i = 0; i < 17; i++)
		{
			g_string_append_printf(out, "| %i ", current->regs[i]);
		}
		g_string_append(out, "\n");
		plugin_write_to_data_pipe(out->str, out->len);
		current = current->next;
	}
}
