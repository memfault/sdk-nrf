/*
 * Copyright (c) 2021 - 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#if defined(TFM_PARTITION_CRYPTO)
#include "common.h"
#include <nrf_cc3xx_platform.h>
#include <nrf_cc3xx_platform_ctr_drbg.h>
#endif

#if defined(NRF_PROVISIONING)
#include "tfm_attest_hal.h"
#endif /* defined(NRF_PROVISIONING) */

#include "tfm_hal_platform.h"
#include "tfm_hal_platform_common.h"
#include "cmsis.h"
#include "uart_stdout.h"
#include "tfm_spm_log.h"
#include "hw_unique_key.h"
#include "config_tfm.h"
#include "exception_info.h"
#include "tfm_arch.h"

#if defined(TFM_PARTITION_CRYPTO)
static enum tfm_hal_status_t crypto_platform_init(void)
{
	int err;

	/* Initialize the nrf_cc3xx runtime */
#if !CRYPTO_RNG_MODULE_ENABLED
	err = nrf_cc3xx_platform_init_no_rng();
#else
#if defined(PSA_WANT_ALG_CTR_DRBG)
	err = nrf_cc3xx_platform_init();
#elif defined (PSA_WANT_ALG_HMAC_DRBG)
	err = nrf_cc3xx_platform_init_hmac_drbg();
#else
	#error "Please enable either PSA_WANT_ALG_CTR_DRBG or PSA_WANT_ALG_HMAC_DRBG"
#endif
#endif

	if (err != NRF_CC3XX_PLATFORM_SUCCESS) {
		return TFM_HAL_ERROR_BAD_STATE;
	}

#if CRYPTO_KEY_DERIVATION_MODULE_ENABLED && \
    !defined(PLATFORM_DEFAULT_CRYPTO_KEYS)
	if (!hw_unique_key_are_any_written()) {
		SPMLOG_INFMSG("Writing random Hardware Unique Keys to the KMU.\r\n");
		hw_unique_key_write_random();
		SPMLOG_INFMSG("Success\r\n");
	}
#endif

	return TFM_HAL_SUCCESS;
}
#endif /* defined(TFM_PARTITION_CRYPTO) */

/* To write into AIRCR register, 0x5FA value must be written to the VECTKEY field,
 * otherwise the processor ignores the write.
 */
#define AIRCR_VECTKEY_PERMIT_WRITE ((0x5FAUL << SCB_AIRCR_VECTKEY_Pos))

static void allow_nonsecure_reset(void)
{
    uint32_t reg_value = SCB->AIRCR;

    /* Clear SCB_AIRCR_VECTKEY value */
    reg_value &= ~(uint32_t)(SCB_AIRCR_VECTKEY_Msk);

    /* Clear SCB_AIRC_SYSRESETREQS value */
    reg_value &= ~(uint32_t)(SCB_AIRCR_SYSRESETREQS_Msk);

    /* Add VECTKEY value needed to write the register. */
    reg_value |= (uint32_t)(AIRCR_VECTKEY_PERMIT_WRITE);

    SCB->AIRCR = reg_value;
}

enum tfm_hal_status_t tfm_hal_platform_init(void)
{
	enum tfm_hal_status_t status;

	status = tfm_hal_platform_common_init();
	if (status != TFM_HAL_SUCCESS) {
		return status;
	}

#if defined(TFM_PARTITION_CRYPTO)
	status = crypto_platform_init();
	if (status != TFM_HAL_SUCCESS)
	{
		return status;
	}
#endif /* defined(TFM_PARTITION_CRYPTO) */

#if defined(NRF_ALLOW_NON_SECURE_RESET)
	allow_nonsecure_reset();
#endif

/* When NRF_PROVISIONING is enabled we can either be in the lifecycle state "provisioning" or
 * "secured", we don't support any other lifecycle states. This ensures that TF-M will not
 * continue booting when an unsupported state is present.
 */
#if defined(NRF_PROVISIONING)
	enum tfm_security_lifecycle_t lcs = tfm_attest_hal_get_security_lifecycle();
	if(lcs != TFM_SLC_PSA_ROT_PROVISIONING && lcs != TFM_SLC_SECURED) {
		return TFM_HAL_ERROR_BAD_STATE;
	}
#endif /* defined(NRF_PROVISIONING) */

	return TFM_HAL_SUCCESS;
}

void tfm_hal_system_halt(void)
{
	/*
	 * Disable IRQs to stop all threads, not just the thread that
	 * halted the system.
	 */
	__disable_irq();

	/*
	 * Enter sleep to reduce power consumption and do it in a loop in
	 * case a signal wakes up the CPU.
	 */
	while (1) {
		__WFE();
	}
}

#define SECUREFAULT_EXCEPTION_NUMBER 7
#define HARDFAULT_EXCEPTION_NUMBER   3
#define BUSFAULT_EXCEPTION_NUMBER    5

void tfm_hal_system_reset(void)
{
#if defined(TFM_EXCEPTION_INFO_DUMP)
	struct exception_info_t *exc_ctx = tfm_exception_info_get_context();

#if defined(TRUSTZONE_PRESENT)
	const uint8_t active_exception_number = (exc_ctx->xPSR & 0xff); 
	const bool securefault_active = (active_exception_number == SECUREFAULT_EXCEPTION_NUMBER);
	const bool busfault_active = (active_exception_number == BUSFAULT_EXCEPTION_NUMBER);

	if ((exc_ctx == NULL) || is_return_secure_stack(exc_ctx->EXC_RETURN) ||
		!(securefault_active || busfault_active)) {
		NVIC_SystemReset();
	}

	/*
	 * If we get here, we are taking a reset path where a fault was generated
	 * from the NS firmware running on the device. If we just reset, it will be
	 * impossible to extract the root cause of the error on the NS side.
	 *
	 * To allow for root cause analysis, let's call the NS HardFault handler.  Any error from
	 * the NS fault handler will land us back in the Secure HardFault handler where we will not
	 * enter this path and simply reset the device.
	 */

	uint32_t *vtor = (uint32_t *)tfm_hal_get_ns_VTOR();

	uint32_t hardfault_handler_fn = vtor[HARDFAULT_EXCEPTION_NUMBER];

	/* bit 0 needs to be cleared to transition to NS */
	hardfault_handler_fn &= ~0x1;

	/* Adjust EXC_RETURN value to emulate NS exception entry */
	uint32_t ns_exc_return = exc_ctx->EXC_RETURN & ~EXC_RETURN_EXC_SECURE;
	/* Update SPSEL to reflect correct CONTROL_NS.SPSEL setting */
	ns_exc_return &= ~(EXC_RETURN_SPSEL);
	CONTROL_Type ctrl_ns;
	ctrl_ns.w = __TZ_get_CONTROL_NS();
	if (ctrl_ns.b.SPSEL) {
		ns_exc_return |= EXC_RETURN_SPSEL;
	}

	__asm volatile("mov lr, %[ns_exc_return]\n"
			"bxns %[hardfault_handler_fn]\n"
			: /* No outputs. */
			: [ns_exc_return] "r"(ns_exc_return),
			  [hardfault_handler_fn] "r"(hardfault_handler_fn));

#endif /* defined(TRUSTZONE_PRESENT) */
#endif /* defined(TFM_EXCEPTION_INFO_DUMP) */

	NVIC_SystemReset();
}
