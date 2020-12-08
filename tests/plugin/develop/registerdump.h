#ifndef FAULTPLUGIN_REGISTERDUMP_H
#define FAULTPLUGIN_REGISTERDUMP_H

#include "faultplugin.h"
#include <qemu/qemu-plugin.h>

/**
 * This enum is the internal value for all Available architectures supported
 */
enum architecture {ARM};

/**
 * init_register_module
 *
 * This function initialises the module. Must be called in setup.
 *
 * @param architecture is used to select the current simulated hardware. See architecture enum for which value represents what
 */
void init_register_module(int architecture);


/**
 * delete_register_module
 *
 * Clear all internal datastructures to free memory. All Data is lost, if this function is called to early
 */
void delete_register_module(void);

/**
 * add_new_registerdump
 *
 * Readout all architecture registers and save value. Then save the value to internal linked list
 *
 * @param tbcount Value that is saved in the tbcount
 *
 * @return return negative value if something went wrong
 */
int add_new_registerdump(uint64_t tbcount);


/**
 * read_register_module
 *
 * Readout structs and write them to data pipe
 */
void read_register_module(void);


#endif
