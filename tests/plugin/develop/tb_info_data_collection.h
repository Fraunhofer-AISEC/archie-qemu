#ifndef TB_INFO_DATA_COLLECTION
#define TB_INFO_DATA_COLLECTION

#include "lib/avl.h"
#include "glib.h"
#include "stdint.h"
#include "qemu/osdep.h"
#include "qemu-common.h"
#include <qemu/plugin.h>
#include <qemu/qemu-plugin.h>

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

/**
 * tb_info_init()
 *
 * This function initialises all global variables used in module
 */
void tb_info_init();

/**
 * tb_info_avl_init()
 *
 * function intialises avl tree for tb info
 */
int tb_info_avl_init();

/**
 * tb_info_free()
 *
 * function to delete the translation block information
 * structs from memory. Also deletes the avl tree
 */
void tb_info_free();

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
int tb_comparison_func(const void *tbl_a, const void *tbl_b, void * tbl_param);

/**
 * plugin_dump_tb_information()
 *
 * Function that reads the tb information structs and prints each to the data pipe. Furthermore writes the command to python that it knows tb information is provided
 *
 *
 */
void plugin_dump_tb_information();

tb_info_t * add_tb_info(struct qemu_plugin_tb *tb);

GString* decode_assembler(struct qemu_plugin_tb *tb);

size_t calculate_bytesize_instructions(struct qemu_plugin_tb *tb);

#endif
