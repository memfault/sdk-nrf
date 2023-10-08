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

#ifdef CONFIG_HW_UNIQUE_KEY_RANDOM
	if (!hw_unique_key_are_any_written()) {
		SPMLOG_INFMSG("Writing random Hardware Unique Keys to the KMU.\r\n");
		err = hw_unique_key_write_random();
		if (err != HW_UNIQUE_KEY_SUCCESS) {
			SPMLOG_DBGMSGVAL("hw_unique_key_write_random failed with error code:", err);
			return TFM_HAL_ERROR_BAD_STATE;
		}
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

#if defined(CONFIG_TFM_ALLOW_NON_SECURE_FAULT_HANDLING)
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

#if defined(TFM_EXCEPTION_INFO_DUMP) && defined(TRUSTZONE_PRESENT)

__attribute__((naked)) static void handle_fault_from_ns(
	uint32_t fault_handler_fn, uint32_t exc_return) {
	/* scrub secure register state before jumping to ns */
	__ASM volatile(
		"mov  lr, r1 \n"
		"movs r1, #0 \n"
		"movs r2, #0 \n"
#if (CONFIG_TFM_FLOAT_ABI >= 1)
		"vmov d0, r1, r2 \n"
		"vmov d1, r1, r2 \n"
		"vmov d2, r1, r2 \n"
		"vmov d3, r1, r2 \n"
		"vmov d4, r1, r2 \n"
		"vmov d5, r1, r2 \n"
		"vmov d6, r1, r2 \n"
		"vmov d7, r1, r2 \n"
		"mrs r2, control \n"
		"bic r2, r2, #4 \n"
		"msr control, r2 \n"
		"isb \n"
#endif
		"ldr  r1, ="M2S(STACK_SEAL_PATTERN)" \n"
		"push {r1, r2} \n"
		"movs r1, #0 \n"
		"movs r3, #0 \n"
		"movs r4, #0 \n"
		"movs r5, #0 \n"
		"movs r6, #0 \n"
		"movs r7, #0 \n"
		"movs r8, #0 \n"
		"movs r9, #0 \n"
		"movs r10, #0 \n"
		"movs r11, #0 \n"
		"movs r12, #0 \n"
		"bic r0, r0, #1 \n"
		"bxns r0 \n"
	);
}

void tfm_hal_system_reset(void)
{
	struct exception_info_t *exc_ctx = tfm_exception_info_get_context();
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

	handle_fault_from_ns(hardfault_handler_fn, ns_exc_return);

	NVIC_SystemReset();
}
#else
void tfm_hal_system_reset(void)
{
	NVIC_SystemReset();
}
#endif /* defined(TRUSTZONE_PRESENT) && defined(TFM_EXCEPTION_INFO_DUMP) */
#endif /* CONFIG_TFM_ALLOW_NON_SECURE_FAULT_HANDLING */
