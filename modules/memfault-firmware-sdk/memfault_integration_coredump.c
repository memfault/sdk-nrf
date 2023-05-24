#include <zephyr/kernel.h>
#include <memfault/components.h>

#include "memfault_etb_trace_capture.h"
#include "memfault_ncs_coredump.h"

#if defined(CONFIG_MEMFAULT_NCS_COREDUMP_REGIONS_CUSTOM)
static sMfltCoredumpRegion s_coredump_regions[MEMFAULT_NCS_COREDUMP_REGIONS];
#endif // CONFIG_MEMFAULT_NCS_COREDUMP_REGIONS_CUSTOM

size_t memfault_ncs_coredump_get_regions(const sCoredumpCrashInfo *crash_info,
					 sMfltCoredumpRegion *regions, size_t num_regions)
{
	// Capture Zephyr regions
	size_t region_idx = memfault_zephyr_coredump_get_regions(crash_info, regions, num_regions);

#if CONFIG_MEMFAULT_NCS_ETB_CAPTURE
	region_idx += memfault_ncs_etb_get_regions(regions[region_idx], num_regions - region_idx);
#endif // CONFIG_MEMFAULT_NCS_ETB_CAPTURE

	return region_idx;
}

#if defined(CONFIG_MEMFAULT_NCS_COREDUMP_REGIONS_CUSTOM)
const sMfltCoredumpRegion *
memfault_platform_coredump_get_regions(const sCoredumpCrashInfo *crash_info, size_t *num_regions)
{
	*num_regions = memfault_ncs_coredump_get_regions(crash_info, s_coredump_regions,
							 ARRAY_SIZE(s_coredump_regions));
	return s_coredump_regions;
}
#endif // CONFIG_MEMFAULT_NCS_COREDUMP_REGIONS_CUSTOM

#if defined(CONFIG_MEMFAULT_NCS_FAULT_HANDLER_CUSTOM)
void memfault_platform_fault_handler(const sMfltRegState *regs, eMemfaultRebootReason reason)
{
	ARG_UNUSED(regs);
	ARG_UNUSED(reason);
#if CONFIG_MEMFAULT_NCS_ETB_CAPTURE
	memfault_ncs_etb_fault_handler();
#endif
}
#endif // CONFIG_MEMFAULT_NCS_FAULT_HANDLER_CUSTOM
