/*
 * Copyright (c) 2021 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <platform/include/tfm_platform_system.h>
#include <cmsis.h>
#include <stdio.h>
#include <pm_config.h>
#include <tfm_ioctl_api.h>
#include <string.h>
#include <arm_cmse.h>

#include "tfm_ioctl_api.h"
#include "tfm_platform_hal_ioctl.h"
#include <tfm_hal_isolation.h>

#include <hal/nrf_gpio.h>
#include "handle_attr.h"

void tfm_platform_hal_system_reset(void)
{
	/* Reset the system */
	NVIC_SystemReset();
}

#if CONFIG_FW_INFO
enum tfm_platform_err_t
tfm_platform_hal_fw_info_service(psa_invec  *in_vec, psa_outvec *out_vec)
{
	const struct fw_info *tfm_info;
	struct tfm_fw_info_args_t *args;
	struct tfm_fw_info_out_t *out;
	enum tfm_hal_status_t status;
	enum tfm_platform_err_t err;
	uint32_t attr = TFM_HAL_ACCESS_WRITABLE |
			TFM_HAL_ACCESS_READABLE |
			TFM_HAL_ACCESS_NS;
	uintptr_t boundary = (1 << HANDLE_ATTR_NS_POS) &
	                      HANDLE_ATTR_NS_MASK;

	if (in_vec->len != sizeof(struct tfm_fw_info_args_t) ||
	    out_vec->len != sizeof(struct tfm_fw_info_out_t)) {
		return TFM_PLATFORM_ERR_INVALID_PARAM;
	}

	args = (struct tfm_fw_info_args_t *)in_vec->base;
	out = (struct tfm_fw_info_out_t *)out_vec->base;

	/* Assume failure, unless valid region is hit in the loop */
	out->result = -1;
	err = TFM_PLATFORM_ERR_INVALID_PARAM;

	if (args->info == NULL) {
		return TFM_PLATFORM_ERR_INVALID_PARAM;
	}

	status = tfm_hal_memory_check(boundary,
				      (uintptr_t)args->info,
				      sizeof(struct fw_info),
				      attr);
	if (status != TFM_HAL_SUCCESS) {
		return TFM_PLATFORM_ERR_INVALID_PARAM;
	}

	tfm_info = fw_info_find((uintptr_t)args->fw_address);
	if (tfm_info != NULL) {
		memcpy(args->info, tfm_info, sizeof(struct fw_info));
		out->result = 0;
		err = TFM_PLATFORM_ERR_SUCCESS;
	}

	return err;
}
#endif


static enum tfm_platform_err_t
tfm_platform_hal_ns_fault_service(const psa_invec *in_vec,
					 const psa_outvec *out_vec)
{
	struct tfm_ns_fault_service_args *args;
	struct tfm_ns_fault_service_out *out;

	if (in_vec->len != sizeof(struct tfm_ns_fault_service_args) ||
	    out_vec->len != sizeof(struct tfm_ns_fault_service_out)) {
		return TFM_PLATFORM_ERR_INVALID_PARAM;
	}

	args = (struct tfm_ns_fault_service_args *)in_vec->base;
	out = (struct tfm_ns_fault_service_out *)out_vec->base;
	out->result = -1;

	switch(args->type)
	{
	case TFM_NS_FAULT_SERVICE_TYPE_SET_HANDLER:
		out->result = ns_fault_service_set_handler(args->set_handler.context, args->set_handler.callback);
		break;
	default:
		return TFM_PLATFORM_ERR_NOT_SUPPORTED;
	}


	return TFM_PLATFORM_ERR_SUCCESS;
}



enum tfm_platform_err_t tfm_platform_hal_ioctl(tfm_platform_ioctl_req_t request,
                                               psa_invec  *in_vec,
                                               psa_outvec *out_vec)
{
	/* Core IOCTL services */
	switch (request) {
	case TFM_PLATFORM_IOCTL_READ_SERVICE:
		return tfm_platform_hal_read_service(in_vec, out_vec);
#if defined(GPIO_PIN_CNF_MCUSEL_Msk)
	case TFM_PLATFORM_IOCTL_GPIO_SERVICE:
		return tfm_platform_hal_gpio_service(in_vec, out_vec);
#endif /* defined(GPIO_PIN_CNF_MCUSEL_Msk) */

#if CONFIG_FW_INFO
	/* Board specific IOCTL services */
	case TFM_PLATFORM_IOCTL_FW_INFO:
		return tfm_platform_hal_fw_info_service(in_vec, out_vec);
#endif
	case TFM_PLATFORM_IOCTL_NS_FAULT:
		return tfm_platform_hal_ns_fault_service(in_vec, out_vec);

	/* Not a supported IOCTL service.*/
	default:
		return TFM_PLATFORM_ERR_NOT_SUPPORTED;
	}
}
