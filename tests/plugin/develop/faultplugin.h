#ifndef QEMU_FAULTPLUGIN
#define QEMU_FAULTPLUGIN


#include <inttypes.h>
#include <glib.h>

enum{ SRAM, FLASH, REGISTER};
enum{ SET0, SET1, TOGGLE};



int plugin_write_to_data_pipe(char *str, size_t len);

#endif
