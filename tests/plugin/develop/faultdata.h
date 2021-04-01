#ifndef QEMU_FAULTPLUGIN_DATA
#define QEMU_FAULTPLUGIN_DATA

#include <inttypes.h>
#include <stdlib.h>
#include "faultplugin.h"

#include <glib.h>

#include <qemu/qemu-plugin.h>


/**
 * init_memory_module()
 *
 * Initialise the global variables.
 * This only makes sure the plugin can deliver a valid response to memory_module_configured
 */
void init_memory_module(void);


/**
 * memory_module_configured()
 *
 * returns 1 if configured otherwise 0
 */
int memory_module_configured(void);

/**
 * init_memory
 * 
 * Initialise the global pointer with the number_of_regions amount of structs.
 *
 * @param number_of_regions: Number of structs to initialise
 */
int init_memory(int number_of_regions);

/**
 * delete_memory_dump
 *
 * Free the complete internal data structure. After this all data is no longer accessible
 */
void delete_memory_dump(void);

/**
 * insert_memorydump_config
 *
 * Initialise one vector element with the memory region, that should be read. 
 * Currently we only read at the end of execution.
 *
 * @param baseaddress: Baseaddress of memory region
 * @param len: length of memory region in bytes
 */
int insert_memorydump_config(uint64_t baseaddress, uint64_t len);

/**
 * read_all_memory
 *
 * Read all client memory regions defined by user.
 */
int read_all_memory(void);

/**
 * read_memoryregion
 *
 * Read one client memory region defined by user 
 *
 * @param memorydump_position: select which region should be read in vector element position
 */
int read_memoryregion(uint64_t memorydump_position);

/**
 * readout_memorydump_dump
 *
 * generate the string for data pipe for one memory region dump taken. It then writes each line directly to data pipe.
 *
 * @param memorydump_position: select which region should be read in vector element
 * @param dump_pos: select which data dump should be written to pipe. Multiple can be taken during the execution of the config.
 */
int readout_memorydump_dump(uint64_t memorydump_position, uint64_t dump_pos);

/**
 * readout_memorydump
 *
 * Call read_memorydump_dump for all available dumps inside the struct. All
 * dumps are printed to data pipe. Also print config for this memorydump to data pipe
 *
 */
int readout_memorydump(uint64_t memorydump_position);

/**
 * readout_all_memorydump
 *
 * This function will send all memorydumps through the data pipe 
 */
int readout_all_memorydump(void);

#endif
