/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MEMFAULT_INTEGRATION_H_
#define MEMFAULT_INTEGRATION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <sys/types.h>

#define MEMFAULT_PROJECT_KEY_LEN 32

#if defined(CONFIG_MEMFAULT_NCS_PROJECT_KEY_SETTINGS)

/** @brief Retrieve the Memfault project key.
 *
 * @return Pointer to the Memfault project key string. Always a valid string.
 */
const char *memfault_ncs_get_project_key(void);

#endif /* defined(CONFIG_MEMFAULT_NCS_PROJECT_KEY_SETTINGS) */

#ifdef __cplusplus
}
#endif

#endif /* MEMFAULT_INTEGRATION_H_ */
