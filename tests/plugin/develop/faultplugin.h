#ifndef QEMU_FAULTPLUGIN
#define QEMU_FAULTPLUGIN


#include <inttypes.h>
#include <glib.h>




int plugin_write_to_data_pipe(char *str, size_t len);

#endif
