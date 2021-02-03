#ifndef TB_EXEC_DATA_COLLECTION
#define TB_EXEC_DATA_COLLECTION


#include <qemu/qemu-plugin.h>
#include "tb_info_data_collection.h"

typedef struct tb_exec_order_t tb_exec_order_t;
typedef struct tb_exec_order_t
{
	tb_info_t *tb_info;
	tb_exec_order_t *prev;
	tb_exec_order_t *next;
}tb_exec_order_t;


void tb_exec_order_init();

/**
 * tb_exec_order_free()
 *
 * free linked list of tb_exec_order_t elements. It does not free the tb_info_t inside.
 * These must be freed seperatly with tb_info_free()
 */
void tb_exec_order_free();

/**
 * plugin_dump_tb_exec_order
 *
 * Print the order of translation blocks executed. Also provide a counter number, that it can be later resorted in python
 */
void plugin_dump_tb_exec_order();

/**
 * tb_exec_data_event
 * 
 * Function to collect the exec data about translation blocks
 *
 * vcpu_index: current index of cpu the callback was triggered from
 * vcurrent: pointer to tb_info struct of the current tb
 */
void tb_exec_data_event(unsigned int vcpu_index, void *vcurrent);
#endif
