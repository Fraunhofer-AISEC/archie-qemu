#ifndef TB_FAULTED_COLLECTION_H
#define TB_FAULTED_COLLECTION_H

#include "faultplugin.h"
#include <stdint.h>

/**
 * tb_faulte_init
 *
 * This function initalises the plugin
 *
 * @param number_faults: Number of faults in the plugin
 */
void tb_faulted_init(int number_faults);

/**
 * tb_faulted_free
 *
 * Free all allocated memory by this module
 */
void tb_faulted_free(void);

/**
 * tb_faulted_register
 *
 * Register a callback for getting a faulted assembly
 */
void tb_faulted_register(uint64_t fault_address);

/**
 * check_tb_faulted
 *
 * Check if a register faulted assembly is available
 *
 * @param tb: Pointer to tb struct given by qemu.
 */
void check_tb_faulted(struct qemu_plugin_tb *tb);

/**
 * dump_tb_faulted_data
 *
 * Write collected data to data pipe
 */
void dump_tb_faulted_data(void);

#endif
