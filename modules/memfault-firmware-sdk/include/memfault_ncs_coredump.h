#ifndef MEMFAULT_NCS_COREDUMP_H_
#define MEMFAULT_NCS_COREDUMP_H_

#include <stddef.h>
#include <memfault/panics/platform/coredump.h>
#include <memfault/ports/zephyr/coredump.h>

#define MEMFAULT_NCS_COREDUMP_REGIONS (MEMFAULT_ZEPHYR_COREDUMP_REGIONS + IS_ENABLED(CONFIG_MEMFAULT_NCS_ETB_CAPTURE))

size_t memfault_ncs_coredump_get_regions(const sCoredumpCrashInfo *crash_info,
					 sMfltCoredumpRegion *regions, size_t num_regions);

#endif // MEMFAULT_NCS_COREDUMP_H_
