/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef __ETB_TRACE_H
#define __ETB_TRACE_H

#include <stdint.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup etb_trace ETB trace
 * @brief Library for capturing instruction traces to Embedded Trace Buffer.
 *
 * @{
 */

#define ETB_BUFFER_SIZE			KB(2)

/**
 * @brief Set up debug unit and start ETB tracing.
 *
 */
void etb_trace_start(void);

/**
 * @brief Stop ETB tracing.
 *
 */
void etb_trace_stop(void);

/**
 * @brief Retrieve ETB trace data to buffer
 *
 * @param[out] buf output buffer
 * @param[in] buf_size size of output buffer
 * @return size_t number of bytes written
 */
size_t etb_data_get(volatile uint32_t *buf, size_t buf_size);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __ETB_TRACE_H */
