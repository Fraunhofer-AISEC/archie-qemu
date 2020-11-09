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

#include "lib/avl.h"

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
	uint64_t trignum;
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
int	exec_callback;
int 	first_tb;

int first_fault_injected;
int tb_counter;
int tb_counter_max;

/*Output TB data structures*/
typedef struct tb_info_t tb_info_t;
typedef struct tb_info_t
{
        uint64_t base_address;
        uint64_t size;
        uint64_t instruction_count;
        GString * assembler;
        uint64_t num_of_exec; // Number of executions(aka a counter)
        tb_info_t *next;
}tb_info_t;

tb_info_t *tb_info_list; 

/*AVL global variables*/
struct avl_table *tb_avl_root;

/**
 * tb_info_free()
 *
 * function to delete the translation block information
 * structs from memory. Also deletes the avl tree
 */
void tb_info_free()
{
	tb_info_t *item;
	while(tb_info_list != NULL)
	{
		item = tb_info_list;
		tb_info_list = tb_info_list->next;
		free(item);
	}
	avl_destroy( tb_avl_root, NULL);
	tb_avl_root = NULL;
}


typedef struct tb_exec_order_t tb_exec_order_t;
typedef struct tb_exec_order_t
{
	tb_info_t *tb_info;
	tb_exec_order_t *prev;
}tb_exec_order_t;

tb_exec_order_t *tb_exec_order_list;
uint64_t num_exec_order;

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
 * tb_comparison_func
 *
 * Needed for avl library. it will determen which element is bigger of type tb_info_t.
 * see documentation of gnuavl lib for more information
 *
 * tbl_a: Element a to be compared
 * tbl_b: Element b to be compared
 * tbl_param: is not used by this avl tree. But can be used to give additional information
 * to the comparison function
 *
 * return if negativ, a is bigger, if possitiv b is bigger. If 0 it is the same element
 */
int tb_comparison_func(const void *tbl_a, const void *tbl_b, void * tbl_param)
{
	//g_autoptr(GString) out = g_string_new("");
	const tb_info_t * tb_a = tbl_a;
	const tb_info_t * tb_b = tbl_b;
	//g_string_printf(out, "[Info]: Compare function called\n");
	//g_string_append_printf(out, "[Info]: a baseaddress: %p, b baseaddress: %p\n",tbl_a, tbl_b);
	//qemu_plugin_outs(out->str);
	if(tb_a->base_address < tb_b->base_address)
	{

		return -1;
	}
	else if(tb_a->base_address > tb_b->base_address) return 1;
	else return 0;
}

/* datastructures für memory access*/
/* avl tree is used for insn address*/
typedef struct mem_info_t mem_info_t;
typedef struct mem_info_t
{
	uint64_t ins_address;
	uint64_t size;
	uint64_t memmory_address;
	char	 direction;
	uint64_t counter;
	mem_info_t *next;
}mem_info_t;

mem_info_t *mem_info_list;

struct avl_table *mem_avl_root;

/**
 * mem_info_free()
 *
 * This function deltes all mem info elemts in the global linkes list mem_info_list.
 * Furthermore it deletes the associated avl tree
 */
void mem_info_free()
{
	mem_info_t *item;
	while(mem_info_list != NULL)
	{
		item = mem_info_list;
		mem_info_list = mem_info_list->next;
		free(item);
	}
	avl_destroy(mem_avl_root, NULL);
	mem_avl_root = NULL;
}

/**
 * mem_comparison_func()
 *
 * This function compares two elements of mem_info_t. It returns signifies which element is bigger
 * needed by gnuavl lib. Please see the gnuavl lib for more information
 *
 * tbl_a: Element a to be compared
 * tbl_b: Element b to be compared
 * tbl_param: Not used. Can be used to give additional information to comparison function
 *
 * return: if negativ a is bigger, if possitiv b is bigger, if zero a = b
 */
int mem_comparison_func(const void *tbl_a, const void *tbl_b, void *tbl_param)
{
	const mem_info_t *mem_a = tbl_a;
	const mem_info_t *mem_b = tbl_b;
	return mem_a->ins_address - mem_b->ins_address;
}

/*Other potential usefull functions needed for gnuavl*/
//void tbl_item_func(void *tbl_item, void *tbl_param)
//void * tbl_copy_func(void *tbl_item, void *tbl_param);
//void tbl_destry_funv(void *tbl_itme, void *tbl_param);

/*QEMU plugin Version control. This is needed to specify for which qemu api version this plugin was build.
 * Qemu woll block, if version is to old to handle incampatibility inside the api
 */
QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;


/**
 * memaccess_data_cb
 *
 * This is the calback, that is called for memaccess by the target cpu.
 * It will search the avl tree, if this memory access is already inside the avl tree. If not it creates the element
 * and inserts it into the tree. then it increments the counter
 *
 * vcpu_index: Index of vcpu that made memory access
 * info: API object needed to query for additional information inside the api
 * vddr: Address in Memory of the memory operation
 * userdata: Data provided by user. In this case it is the address of the instruction, that triggerd the memory operation
 */
static void memaccess_data_cb(unsigned int vcpu_index, qemu_plugin_meminfo_t info, uint64_t vddr, void *userdata)
{
	mem_info_t tmp;
	tmp.ins_address = (uint64_t)(userdata);
	mem_info_t *mem_access = avl_find(mem_avl_root,&tmp);
	if(mem_access == NULL)
	{
		mem_access = malloc(sizeof(mem_info_t));
		mem_access->ins_address = userdata;
		mem_access->size = qemu_plugin_mem_size_shift(info);
		mem_access->memmory_address = vddr;
		mem_access->direction = qemu_plugin_mem_is_store(info);
		mem_access->counter = 0;
		avl_insert(mem_avl_root, mem_access);
		mem_access->next = mem_info_list;
		mem_info_list = mem_access;
	}
	mem_access->counter++;

}

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
	g_string_append_printf(out, "[Info]: Starting argparsing\n");
	if(argc != 3)
	{
		g_string_append_printf(out, "[ERROR]: Not the right ammount of arguments! %i\n", argc);
		return -1;
	}
	g_string_append_printf(out, "[Info]: Start readout of control fifo %s\n", *(argv+0));
	pipes->control = open(*(argv+0), FIFO_READ);
	g_string_append_printf(out, "[Info]: Start readout of config fifo %s\n", *(argv+1));
	pipes->config = open(*(argv+1), FIFO_READ);
	g_string_append_printf(out, "[Info]:Start readout of data fifo %s\n", *(argv+2));
	pipes->data = open(*(argv+2), FIFO_WRITE);
	return 0;
}

/**
 * char_to_uint64()
 *
 * Converts the characters of string provided by c from ascii hex to ascii
 *
 * c: pointer to string
 * size_c: length of string
 *
 * return number converted
 */
uint64_t char_to_uint64(char *c, int size_c)
{
	g_autoptr(GString) out = g_string_new("");
	uint64_t tmp = 0;
	int i = 0;
	g_string_printf(out, "[Info]: This is the conversion function: ");
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
 * print_assembler
 *
 * print assembler to console from translation block
 */
void print_assembler(struct qemu_plugin_tb *tb)
{
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "\n");

	for(int i = 0; i < tb->n; i++)
	{
		struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
		g_string_append_printf(out, "%8lx ", insn->vaddr);
		g_string_append_printf(out, "%s\n", qemu_plugin_insn_disas( insn));
	}
	qemu_plugin_outs(out->str);
}

/**
 * decode_assembler()
 *
 * build string that is later provided to python. !! is the replacement for \n, as this would directly affect decoding.
 * 
 * tb: tb struct, that contains the information needed to get the assembler for the instructions inside the translation block.
 *
 * return: gstring object, that contains the assembly instructions. The object needs to be deleted by the function that called this function
 */
GString* decode_assembler(struct qemu_plugin_tb *tb)
{
	GString* out = g_string_new("");
	
	for(int i = 0; i < tb->n; i++)
	{
		struct qemu_plugin_insn * insn = qemu_plugin_tb_get_insn(tb, i);
		g_string_append_printf(out, "[%8lx]: %s !!", insn->vaddr, qemu_plugin_insn_disas(insn));
	}
	return out;
}

/**
 *
 * qemu_setup_config
 *
 * This function reads the config from the config pipe. It will only read one fault configuration.
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
	g_string_printf(out, "[Info]: Start redout of FIFO\n");
	for(int i = 0; i < 7; i++)
	{
		g_string_append_printf(out, "[Info]: Parameter %i\n", i);
		//qemu_plugin_outs(out->str);
		ssize_t readout = 0;
		ssize_t read_bytes = 0;
		while(target_len > read_bytes)
		{
			readout = read(pipes->config, buf + readout, target_len - readout );
			g_string_append_printf(out, "[DEBUG]: readout %li, target_len %li \n", readout, target_len);
			if(readout == -1)
			{
				g_string_append_printf(out, "[DEBUG]: Value is negativ, Somthing happend in read: %s\n", strerror(errno));
				g_string_append_printf(out, "[DEBUG]: File descriptor is : %i\n", pipes->config);
				//qemu_plugin_outs(out->str);
				//g_string_printf(out, "  \n");
			}
			else
			{
				read_bytes += readout;
				readout = 0;
			}
		}
		g_string_append_printf(out, "[Info]: done readout of pipe\n");
		//qemu_plugin_outs(out->str);
		switch(i)
		{
			case 0:
				fault_address = char_to_uint64(buf, 8);
				target_len = 8;
				g_string_append_printf(out, "[Info]: fault address: 0x%lx\n", fault_address);
				break;
			case 1:
				fault_type = char_to_uint64(buf, 8);
				target_len = 8;
				g_string_append_printf(out, "[Info]: fault address: 0x%lx\n", fault_type);
				break;
			case 2:
				fault_model = char_to_uint64(buf, target_len);
				target_len = 8;
				g_string_append_printf(out, "[Info]: fault address: 0x%lx\n", fault_model);
				break;
			case 3:
				fault_lifetime = char_to_uint64(buf, target_len);
				target_len = 16;
				g_string_append_printf(out, "[Info]: fault address: 0x%lx\n", fault_lifetime);
				break;
			case 4:
				//fault_mask = buf[15] << 15*8 | buf[14] << 14*8 | buf[13] << 13*8 | buf[12] << 12*8 | buf[11] << 11*8 | buf[10] << 10*8 | buf[9] << 9*8 | buf[8] << 8*8 | buf[7] << 7*8 | buf[6] << 6*8 | buf[5] << 5*8 | buf[4] << 4*8 |buf[3] << 3*8 | buf[2] << 2*8 | buf[1] << 1*8 | buf[0] << 0*8;
				g_string_append(out, "[Info]: fault mask: ");
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
				g_string_append_printf(out, "[Info]: fault address: 0x%lx\n", fault_trigger_address);
				break;
			case 6:
				fault_trigger_hitcounter = char_to_uint64(buf, target_len);
				target_len = 8;
				g_string_append_printf(out, "[Info]: fault address: 0x%lx\n", fault_trigger_hitcounter);
				break;
		}
	}
	g_string_append(out, "[Info]: Fault pipe read done\n");
	qemu_plugin_outs(out->str);
	return add_fault(fault_address, fault_type, fault_model, fault_lifetime, fault_mask, fault_trigger_address, fault_trigger_hitcounter);
}


/**
 * add fault
 *
 * This function appends one fault to the linked list. 
 *
 * fault_address: address of fault
 * fault_type: type of fault. see enum on implemented targets
 * fault_model: model of fault. see enum on implemented fault models
 * fault_lifetime: How long should the fault reside. 0 means indefinitely
 * fault_mask: bitmask on which bits should be targeted.
 * fault_trigger_address: Address of trigger location. Fault will be injected if this location is reached
 * fault_trigger_hitcounter: set how many times the location needs to be reached before the fault is injected
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

/**
 * set_fault_trigger_num
 *
 * Function sets the trigger num field. This is done to sepperate between two triggers with the same address
 */
void set_fault_trigger_num(fault_list_t * current, uint64_t trignum)
{
	current->fault.trigger.trignum = trignum;
}

fault_list_t * get_fault_struct_by_trigger(uint64_t fault_trigger_address, uint64_t fault_trigger_number)
{
	fault_list_t * current = first_fault;
	while(current != NULL)
	{
		if(current->fault.trigger.address == fault_trigger_address)
		{
			if(current->fault.trigger.trignum == fault_trigger_number)
			{
				return current;
			}
		}
		current = current->next;
	}
	return NULL;
}

fault_list_t * get_fault_struct_by_exec(uint64_t exec_callback_address)
{
	fault_list_t * current = first_fault;
	while(current != NULL)
	{
		if(current->fault.address == exec_callback_address)
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
	g_string_printf(out, "[Info]: Calculate number of faults .......");
	/*Select first element of list*/
	fault_list_t * current = first_fault;
	int i = 0;
	/*traverse list*/
	while(current != NULL)
	{
		i++;
		current = return_next(current);
	}
	g_string_append_printf(out, "%i\n",i);
	if(i == 0)
	{
		g_string_append(out, "[ERROR]:No fault found!\n");
		qemu_plugin_outs(out->str);
		return -1;
	}
	/* Reset back to firs element*/
	current = first_fault;
	fault_number = i;
	g_string_append_printf(out, "[DEBUG]: Fault number %i\n", fault_number);
	/* Reserve Memory for "Vector"*/
	fault_trigger_addresses = malloc(sizeof(uint64_t) * fault_number);
	fault_addresses = malloc(sizeof(uint64_t) * fault_number);
	if(fault_trigger_addresses == NULL || fault_addresses == NULL)
	{
		g_string_append_printf(out, "[ERROR]: malloc failed here in registerfaulttrigger\n");
		qemu_plugin_outs(out->str);
		return -1;
	}
	g_string_append(out, "[Info]: Start registering faults\n");
	for(int j = 0; j < i; j++)
	{
		/* Fill Vector with value*/
		*(fault_trigger_addresses + j) = get_fault_trigger_address(current);
		set_fault_trigger_num(current, j);
		*(fault_addresses + j) = 0;	
		g_string_append_printf(out, "[Fault]: fault trigger addresses: %p\n", fault_trigger_addresses+j);
		g_string_append_printf(out, "[Fault]: fault addresses: %p\n", fault_addresses+j);
		current = return_next(current);	
	}
	qemu_plugin_outs(out->str);
	return 0;	
}

/**
 * delete_fault_trigger_address()
 *
 * delete the vector containing the fault triggers
 */
void delete_fault_trigger_addresses()
{
	free(fault_trigger_addresses);
}

/**
 * process_set1_memory
 *
 * Read memory, then apply set1 according to mask, then write memory back
 * 
 * address: baseaddress of lowest byte
 * mask: mask containing which bits need to be flipped to 1
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
	ret += cpu_memory_rw_debug( cpu, address, value, 16, 1);
	if (ret < 0)
	{
		qemu_plugin_outs("Somthing went wrong in read/write to cpu in process_set0_memory\n");
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
void process_set0_memory(uint64_t address, uint8_t  mask[])
{
	uint8_t value[16];
	CPUState *cpu = current_cpu;
	int ret;
	/*TODO Return validate*/
	ret = cpu_memory_rw_debug( cpu, address, value, 16, 0);
	for(int i = 0; i < 16; i++)
	{
		value[i] = value[i] & ~(mask[i]);
	}
	ret += cpu_memory_rw_debug( cpu, address, value, 16, 1);
	if (ret < 0)
	{
		qemu_plugin_outs("Somthing went wrong in read/write to cpu in process_set0_memory\n");
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
void process_toggle_memory(uint64_t address, uint8_t  mask[])
{
	uint8_t value[16];
	CPUState *cpu = current_cpu;
	int ret;
	ret = cpu_memory_rw_debug( cpu, address - 1, value, 16, 0);
	for(int i = 0; i < 16; i++)
	{
		value[i] = value[i] ^ mask[i];
	}
	ret += cpu_memory_rw_debug( cpu, address - 1, value, 16, 1);
	if (ret < 0)
	{
		qemu_plugin_outs("Somthing went wrong in read/write to cpu in process_set0_memory\n");
	}
}

/**
 * register_exec_callback
 *
 * This function is called, when the exec callback is needed. This vector is used, if fault is inserted.
 * It is checked to locate the faults struct, that where inserted
 */
void register_exec_callback(uint64_t address)
{
	if(exec_callback == fault_number )
	{
		qemu_plugin_outs("ERROR: Reached max exec callbacks. Something went totaly wrong!\n");
		return;
	}
	*(fault_addresses + exec_callback) = address;
	exec_callback++;
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
			process_set0_memory(current->fault.address, current->fault.mask);
			break;
		case SET1:
			g_string_append_printf(out, "Set 1 fault to Address %lx\n", current->fault.address);
			process_set1_memory(current->fault.address, current->fault.mask);
			break;
		case TOGGLE:
			g_string_append_printf(out, "Toggle fault to Address %lx\n", current->fault.address);
			process_toggle_memory(current->fault.address, current->fault.mask);
			break;
		default:
			break;
	}
	qemu_plugin_outs(out->str);

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
			register_exec_callback(current->fault.address);
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
		first_fault_injected = 1;
	}
}



/**
 * handle_first_tb_fault_insertion
 *
 * This function is called in the first used tb block
 * This function is maybe a TODO
 */
void handle_first_tb_fault_insertion()
{

	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "Look into if we need to insert a fault!\n");
	fault_list_t * current = first_fault;
	qemu_plugin_outs(out->str);
	g_string_printf(out, " ");
	while(current != NULL)
	{
		if(current->fault.trigger.hitcounter == 0 && current->fault.type == FLASH )
		{
			qemu_plugin_outs("Insert first fault\n");
			inject_fault(current);
			*(fault_trigger_addresses + current->fault.trigger.trignum) = 0; //Remove trigger from vector
			/*for(int i = 0; i < fault_number; i++)
			{
				if(*(fault_trigger_addresses + i) == current->fault.trigger.address)
				{
					*(fault_trigger_addresses + i) = 0; //Remove trigger from vector
				}
			}*/
		}
		current = return_next( current);
	}
	qemu_plugin_outs(out->str);

}


/*
 * calculate_bytesize_instructions
 *
 * Function to calculate size of TB. It uses the information of the tb and the last insn to determen the bytesize of the instructions inside the translation block
 */
size_t calculate_bytesize_instructions(struct qemu_plugin_tb *tb)
{
	g_autoptr(GString) out = g_string_new("");
	
	struct qemu_plugin_insn * insn_first = qemu_plugin_tb_get_insn(tb, 0);
	struct qemu_plugin_insn * insn_last = qemu_plugin_tb_get_insn(tb, tb->n -1);
	uint64_t size = (insn_last->vaddr - insn_first->vaddr) + insn_last->data->len;
	g_string_printf(out, "[CALC]: tb instruction size is %li \n", size);

	qemu_plugin_outs(out->str);
	return (size_t) size;
}

/**
 * trigger_insn_cb
 *
 * This function is registered on insn exec of trigger
 * It will determine, if the current fault should be injected or needs to wait. If yes will call the fault injection function 
 */
void trigger_insn_cb(unsigned int vcpu_index, void *vcurrent)
{
	fault_list_t *current = (fault_list_t *) vcurrent;

	//current->fault.trigger.hitcounter = current->fault.trigger.hitcounter - 1;
	if(current->fault.trigger.hitcounter != 0)
	{
		current->fault.trigger.hitcounter = current->fault.trigger.hitcounter - 1;
		qemu_plugin_outs("Trigger eval function reached\n");
		if(current->fault.trigger.hitcounter == 0 )
		{
			/*Trigger met, Inject fault*/
			qemu_plugin_outs("Trigger reached level, inject fault\n");
			inject_fault(current);
			*(fault_trigger_addresses + current->fault.trigger.trignum) = 0;
		}
	}
	else
	{
		qemu_plugin_outs("[ERROR]: The hitcounter was already 0\n");
	}
}

/**
 * TODO remove?
 */
void tb_exec_cb(unsigned int vcpu_index, void *userdata)
{
	fault_list_t *current = (fault_list_t *) userdata;

	//qemu_plugin_outs("[TB] exec tb exec cb\n");
}

/**
 *  evaluate_trigger
 *
 *  This function takes the trigger address number and evaluates the trigger condition
 *  
 *  tb: Struct containing information about the translation block
 *  trigger_address_num: the location in the trigger vector. is used to find the current fault
 */
void evaluate_trigger(struct qemu_plugin_tb *tb,int trigger_address_number)
{

	/*Get fault description*/
	fault_list_t *current = get_fault_struct_by_trigger((uint64_t) *(fault_trigger_addresses + trigger_address_number), trigger_address_number);
	/* Trigger tb met, now registering callback for exec to see, if we need to inject fault*/
	for(int i = 0; i < tb->n; i++)
	{
		struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
		if((current->fault.trigger.address >= qemu_plugin_insn_vaddr(insn))&&(current->fault.trigger.address < qemu_plugin_insn_vaddr(insn) + qemu_plugin_insn_size(insn)))
		{
			/* Trigger address met*/
			qemu_plugin_outs("[TB] Reached injection of callback\n");
			qemu_plugin_register_vcpu_insn_exec_cb(insn, trigger_insn_cb, QEMU_PLUGIN_CB_RW_REGS, current);
			//qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_cb, QEMU_PLUGIN_CB_RW_REGS, current);

		}
	}
	print_assembler(tb);
}

// Calback for instructin exec TODO: remove?
void insn_exec_cb(unsigned int vcpu_index, void *userdata)
{
	g_autoptr(GString) out = g_string_new("");
	g_string_append(out, "Next instruciont\n");
	g_string_append_printf(out, " reg[0]: %08x\n", read_arm_reg(0));

	qemu_plugin_outs(out->str);
}

/**
 * eval_exec_callback
 *
 * This function evaluates if the exec callback is needed to be registered. Also makes sure that fault is reverted, if lifetime is zero
 *
 * tb: Information provided by the api about the translated block
 * exec_callback_number: Position in vector. Needed to find fault struct
 */
void eval_exec_callback(struct qemu_plugin_tb *tb, int exec_callback_number)
{

	fault_list_t * current = get_fault_struct_by_exec((uint64_t) *(fault_addresses + exec_callback_number));

	if(current->fault.lifetime == 0)
	{

		*(fault_addresses + exec_callback_number) = 0;
		qemu_plugin_outs("Remove exec callback\n");
		//TODO: revert fault
	}
	else
	{
		/* Register exec callback*/
		for(int i = 0; i < tb->n; i++)
		{
			struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
			if((current->fault.address >= qemu_plugin_insn_vaddr(insn))&&(current->fault.address <= qemu_plugin_insn_vaddr(insn) + qemu_plugin_insn_size(insn)))
			{
				/* Trigger address met*/
				qemu_plugin_outs("Register exec callback\n");
				//qemu_plugin_register_vcpu_insn_exec_cb(insn, trigger_insn_cb, QEMU_PLUGIN_CB_RW_REGS, current);
			}	
		}
	}
}


/**
 * plugin_write_to_data_pipe
 *
 * Function that handles the write to the data pipe
 * 
 * str: pointer to string to be printed
 * len: length of string to be printed
 * 
 * return negativ if failed
 */
int plugin_write_to_data_pipe(char *str, size_t len)
{
	g_autoptr(GString) out = g_string_new("");
	ssize_t ret = 0;
	while(len != 0)
	{
		ret = write( pipes->data, str, len);
		if(ret == -1)
		{
			g_string_printf(out, "[DEBUG]: output string was: %s\n", str);
			g_string_append_printf(out, "[DEBUG]: Value is negativ, Somthing happend in write: %s\n", strerror(errno));
			g_string_append_printf(out, "[DEBUG]: File descriptor is : %i\n", pipes->data);
			qemu_plugin_outs(out->str);
			return -1;
		}
		str = str + ret;
		len = len - ret;
	}
	return 0;
}


/**
 * plugin_dump_tb_information()
 *
 * Function that reads the tb information structs and prints each to the data pipe. Furthermore writes the command to python that it knows tb information is provided
 *
 *
 */
void plugin_dump_tb_information()
{
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "$$$[TB Information]:\n");
	plugin_write_to_data_pipe(out->str, out->len);
	tb_info_t *item = tb_info_list;
	while(item != NULL)
	{
		g_string_printf(out, "$$0x%lx | 0x%lx | 0x%lx | 0x%lx | %s \n", item->base_address, item->size, item->instruction_count, item->num_of_exec, item->assembler->str );
		plugin_write_to_data_pipe(out->str, out->len);
		item = item->next;
	//	free(tb_info_list);
	//	tb_info_list = item;
	}
		
	//tb_info_t *tb_info_list;
}

/**
 * plugin_dump_tb_exec_order
 *
 * Print the order of translation blocks executed. Also provide a counter number, that it can be later resorted in python
 */
void plugin_dump_tb_exec_order()
{
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "$$$[TB Exec]:\n");
	plugin_write_to_data_pipe(out->str, out->len);
	tb_exec_order_t *item =  tb_exec_order_list;
	while(item != NULL)
	{
		g_string_printf(out, "$$ 0x%lx | %li \n", item->tb_info->base_address, num_exec_order);
		plugin_write_to_data_pipe(out->str, out->len);
		item = item->prev;
		num_exec_order--;
		//free(tb_exec_order_list);
		//tb_exec_order_list = item;
	}
}

/**
 * plugin_dump_mem_information
 *
 * Write collected inforation about the memory accesses to data pipe
 */
void plugin_dump_mem_information()
{
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "$$$[Mem Information]:\n");
	plugin_write_to_data_pipe(out->str, out->len);

	mem_info_t *item = mem_info_list;
	while(item != NULL)
	{
		g_string_printf(out, "$$ 0x%lx | 0x%lx | 0x%lx | 0x%x | 0x%lx \n", item->ins_address, item->size, item->memmory_address, item->direction, item->counter);
		plugin_write_to_data_pipe(out->str, out->len);
		item = item->next;
		//free(mem_info_list);
		//mem_info_list = item;
	}
}

/**
 * plugin_end_information_dump
 *
 * This function first writes all collected data to data pipe, then deletes all information structs
 * Then it will cause a segfault to crash qemu to end it for the moment
 */
void plugin_end_information_dump()
{
	int *error = NULL;
	qemu_plugin_outs("[DEBUG]: Start printing to data pipe tb information\n");
	plugin_dump_tb_information();
	qemu_plugin_outs("[DEBUG]: Start printing to data pipe tb exec\n");
	plugin_dump_tb_exec_order();
	qemu_plugin_outs("[DEBUG]: Start printing to data pipe tb mem\n");
	plugin_dump_mem_information();
	qemu_plugin_outs("[DEBUG]: Information now in pipe, start deleting information in memory\n");
	qemu_plugin_outs("[DEBUG]: Delete tb_info\n");
	tb_info_free();
	qemu_plugin_outs("[DEBUG]: Delete tb_exec\n");
	tb_exec_order_free();
	qemu_plugin_outs("[DEBUG]: Delte mem\n");
	mem_info_free();
	qemu_plugin_outs("[DEBUG]: This is the End\n");
	plugin_write_to_data_pipe("$$$[END]", 8);
	//Insert deliberate error to cancle exec
	//TODO Build good exit to qemu
	//while(1);
	*error = 0;
	//mem_info_free();
	//Now we will start to dump information
	//
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
	tb_info->num_of_exec++;
	tb_exec_order_t *last = malloc(sizeof(tb_exec_order_t));
	last->tb_info = tb_info;
	last->prev = tb_exec_order_list;
	tb_exec_order_list = last;
	num_exec_order++;
	//TODO
	//Build Abbortion logic here. Because this will be executed every tb
	if(first_fault_injected == 1)
	{
		if(tb_counter == tb_counter_max)
		{
			plugin_end_information_dump();
		}
		tb_counter++;
	}
	//
//DEBUG	
//	g_autoptr(GString) out = g_string_new("");
//	g_string_printf(out, "[TB_exec]: ID: %x, Execs %i\n", tb_info->base_address, tb_info->num_of_exec);
//	qemu_plugin_outs(out->str);
}


/**
 * handle_tb_translate_event
 *
 * This function takes the tb struct and triggers the needed evaluation functions
 *
 */
void handle_tb_translate_event(struct qemu_plugin_tb *tb)
{
	fault_list_t * current = first_fault;
	size_t tb_size = calculate_bytesize_instructions(tb);
	qemu_plugin_outs("Reached tb handle function\n");
	qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_cb, QEMU_PLUGIN_CB_RW_REGS, NULL);
	/**Verify, that no trigger is called*/
	for( int i = 0; i < fault_number; i++)
	{
		if((tb->vaddr <= *(fault_trigger_addresses + i))&&((tb->vaddr + tb_size) >= *(fault_trigger_addresses+i)))
		{
			g_autoptr(GString) out = g_string_new("");
			g_string_printf(out, "Met trigger address: %lx\n", *(fault_trigger_addresses + i) );
			qemu_plugin_outs(out->str);
			evaluate_trigger( tb, i);
		}
	}
	/* Verify, if exec callback is requested */
	for(int i = 0; i < exec_callback; i++)
	{
		if((tb->vaddr < *(fault_addresses + i)) && ((tb->vaddr + tb_size) > *(fault_addresses + i)))
		{
			g_autoptr(GString) out = g_string_new("");
			g_string_printf(out, " Reached exec callback event\n");
			qemu_plugin_outs(out->str);
			eval_exec_callback(tb, i);
		}
	}
}

/**
 * handle_tb_translate_data
 *
 * Find the current info struct of translation blocks inside avl tree.
 * If there is no strict in avl, create struct and place it into avl.
 * Also register tb_callback_event to fill in runtime information
 *
 * tb: API struct containing information about the translation block
 */
void handle_tb_translate_data(struct qemu_plugin_tb *tb)
{
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "\n");
	//TODO
	//virt1 ist id der tb
	tb_info_t tmp;
	tmp.base_address = tb->vaddr;
	g_string_append_printf(out, "[TB Info]: Search TB......");
	//qemu_plugin_outs(out->str);
	tb_info_t *tb_information = (tb_info_t *) avl_find(tb_avl_root, &tmp); 	
	if(tb_information == NULL)
	{
		tb_information = malloc(sizeof(tb_info_t));
		if(tb_information == NULL)
		{
			return;
		}
		tb_information->base_address = tb->vaddr;
		tb_information->instruction_count = tb->n;
		tb_information->assembler = decode_assembler(tb);
		tb_information->num_of_exec = 0;
		tb_information->size = calculate_bytesize_instructions(tb);
		tb_information->next = tb_info_list;
		tb_info_list = tb_information;
		g_string_append(out, "Not Found\n");
		if( avl_insert(tb_avl_root, tb_information) != NULL)
		{
			qemu_plugin_outs("[ERROR]: Somthing went wrong in avl instert");
			return;
		}
		else
		{
			if(avl_find(tb_avl_root, &tmp) != tb_information)
			{
				qemu_plugin_outs("[ERROR]: Conntent changed!");
				return;
			}
		}
		g_string_append(out, "[TB Info]: Done insertion into avl\n");
		//qemu_plugin_outs(out->str);
	}
	else
	{
		g_string_append(out, "Found\n");
	}

	qemu_plugin_register_vcpu_tb_exec_cb(tb, tb_exec_data_event, QEMU_PLUGIN_CB_RW_REGS, tb_information);
	for(int i = 0; i < tb->n; i++)
	{
		struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);

		qemu_plugin_register_vcpu_mem_cb( insn, memaccess_data_cb, QEMU_PLUGIN_CB_RW_REGS, QEMU_PLUGIN_MEM_RW, insn->vaddr);
	}
	
	//DEBUG
	GString *assembler = decode_assembler(tb);
	g_string_append_printf(out, "[TB Info] tb id: %8lx\n[TB Info] tb size: %i\n[TB Info] Assembler:\n%s", tb->vaddr, tb->n, assembler->str);
	g_string_free(assembler, TRUE);


	qemu_plugin_outs(out->str);
	
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

	g_string_append_printf(out, "[TB] Virt1 value: %8lx\n", tb->vaddr);

	qemu_plugin_outs(out->str);
	g_string_printf(out, " ");
	if(first_tb != 0)
	{
		g_string_append_printf(out, "[TB] Reached normal tb\n\n");
		qemu_plugin_outs(out->str);
		g_string_printf(out, " ");
		handle_tb_translate_event( tb);

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
	handle_tb_translate_data(tb);
}

void readout_controll_pipe(GString *out)
{
	char c = ' ';
	int ret = 0;
	while(c != '\n')
	{
		ret = read(pipes->control, &c, 1);
		if(ret != 1)
		{
			qemu_plugin_outs("[DEBUG]: Readout config no character found or too much read\n");
			c = ' ';
		}
		else
		{
			g_string_append_c(out, c);
		}
	}
	//qemu_plugin_outs(out->str);
	
}

int readout_controll_mode(GString *conf)
{
	if(strstr(conf->str, "[Config]"))
	{
		return 1;
	}
	if(strstr(conf->str, "[Start]"))
	{
		return 2;
	}
	return -1;
}

int readout_controll_config(GString *conf)
{
	if(strstr(conf->str, "max_duration: "))
	{
		//convert number in string to number
		tb_counter_max = strtoimax(strstr(conf->str,"max_duration: ") + 13, NULL, 0 );
		return 1;
	}
	if(strstr(conf->str, "num_faults: "))
	{
		//convert number in string to number
		fault_number = strtoimax(strstr(conf->str,"num_faults: ") + 11, NULL, 0 );
		return 1;
	}
	return -1;
	
}

int readout_controll_qemu()
{
	g_autoptr(GString) conf = g_string_new("");
	char c = ' ';
	int ret = 0;
	int mode = 0;
	while(mode != 2)
	{
		g_string_printf(conf, " ");
		readout_controll_pipe(conf);
		if(strstr(conf->str, "$$$"))
		{
			mode = readout_controll_mode(conf);
			if(mode == -1)
			{
				qemu_plugin_outs("[ERROR]: Unknown Command\n");
				return -1;
			}
		}
		else
		{
			if(strstr(conf->str, "$$"))
			{
				if(mode == 1)
				{
					if(readout_controll_config(conf) == -1)
					{
						qemu_plugin_outs("[ERROR]: Unknown Parameter\n");
						return -1;
					}
				}
			}
		}

	}
	qemu_plugin_outs("[DEBUG]: Finished readout controll. Now start readout of config\n");
	for(int i = 0; i < fault_number; i++)
	{
		if(qemu_setup_config() < 0)
		{
			qemu_plugin_outs("[ERROR]: Somthing went wrong in readout of config pipe\n");
			return -1;
		}
	}
	return 1;
}


/**
 *
 * qemu_plugin_install
 *
 * This is the first called function.
 * It needs to setup all needed parts inside the plugin
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
	tb_avl_root = NULL;
	tb_info_list = NULL;
	tb_exec_order_list = NULL;
	num_exec_order = 0;
	mem_info_list = NULL;
	exec_callback = 0;
	first_tb = 0;
	first_fault_injected = 0;
	tb_counter = 0;
	tb_counter_max = 1000;
	g_autoptr(GString) out = g_string_new("");
	g_string_printf(out, "QEMU INjection Plugin\n Current Target is %s\n", info->target_name);
	g_string_append_printf(out, "Current Version of QEMU Plugin is %i, Min Version is %i\n", info->version.cur, info->version.min);
	if(strcmp(info->target_name, "arm") < 0)
	{
		g_string_append(out, "[ERROR]: Arbort plugin, as this architecture is currently not supported!\n");
		qemu_plugin_outs(out->str);
		return -1;
	}
	pipes = malloc(sizeof(fifos_t));
	if(pipes == NULL)
	{	g_string_append(out, "[ERROR]: Pipe struct not malloced\n");
		goto ABBORT;
	}
	pipes->control = -1;
	pipes->config = -1;
	pipes->data = -1;
	/*Start Argparsing and open up the Fifos*/
	if(parse_args(argc, argv, out) != 0)
	{
		g_string_append_printf(out, "[ERROR]: Initialisation of FIFO failed!\n");
		qemu_plugin_outs(out->str);
		return -1;
	}
	g_string_append_printf(out, "[Info]: Initialisation of FIFO.......Done!\n");
	g_string_append_printf(out, "[Info]: Readout config FIFO\n");
	qemu_plugin_outs(out->str);
	g_string_printf(out, " ");
//	if( qemu_setup_config() < 0)
	if( readout_controll_qemu() < 0)
	{
		goto ABBORT;
	}
	g_string_append_printf(out, "[Info]: Linked list entry address: %p\n", first_fault);	

	g_string_append_printf(out, "[Info]: Register fault trigger addresses\n");
	qemu_plugin_outs(out->str);
	g_string_printf(out, " ");
	if(register_fault_trigger_addresses() < 0 )
	{
		goto ABBORT;
	}
	g_string_append_printf(out, "[Info]: Number of triggers: %i\n", fault_number);
	g_string_append(out, "[Info]: Register VCPU tb trans callback\n");
	qemu_plugin_register_vcpu_tb_trans_cb( id, vcpu_translateblock_translation_event);
	g_string_append(out, "[Info]: Initialise TB avl tree ....");
	tb_avl_root = avl_create( &tb_comparison_func, NULL, NULL);
	if(tb_avl_root == NULL)
	{
		g_string_append(out, "ERROR\n[ERROR] TB avl tree initialisation failed\n");
		goto ABBORT;
	}
	g_string_append(out, "Done\n");
	g_string_append(out, "[Info] Initialise mem avl tree ....");
	mem_avl_root = avl_create( &mem_comparison_func, NULL, NULL);
	if(mem_avl_root == NULL)
	{
		g_string_append(out, "ERROR\n[ERROR] mem avl tree initialisation failed");
		goto ABBORT;
	}
	g_string_append(out, "Done\n");
	g_string_append_printf(out, "[Start]: Reached end of Initialisation, starting guest now\n");
	qemu_plugin_outs(out->str);
	return 0;
ABBORT:
	if(mem_avl_root != NULL)
	{
		avl_destroy(mem_avl_root, NULL);
	}
	if(tb_avl_root != NULL)
	{
		avl_destroy(tb_avl_root, NULL);
	}
	delete_fault_trigger_addresses();
	delete_fault_queue();
	g_string_append(out, "[ERROR]: Somthing went wrong. Abborting now!\n");
	qemu_plugin_outs(out->str);
	return -1;
}
