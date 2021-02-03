#ifndef QEMU_FAULTPLUGIN
#define QEMU_FAULTPLUGIN


#include <inttypes.h>
#include <glib.h>
#include "fault_list.h"

enum{ SRAM, FLASH, REGISTER};
enum{ SET0, SET1, TOGGLE};


int register_live_faults_callback(fault_list_t *fault);

void invalidate_fault_trigger_address(int fault_trigger_number);

int plugin_write_to_data_pipe(char *str, size_t len);
 

#endif
