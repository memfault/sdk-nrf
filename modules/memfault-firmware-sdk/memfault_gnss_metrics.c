/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <modem/location.h>
#include "memfault/metrics/metrics.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(memfault_ncs_metrics, CONFIG_MEMFAULT_NCS_LOG_LEVEL);

static bool session_in_progress = false;
static location_event_handler_t real_location_event_handler;

static void memfault_location_event_handler(const struct location_event_data *event_data);
static void memfault_gnss_metrics_start_fix_session(void);
static void memfault_gnss_metrics_stop_fix_session(const struct location_event_data *event_data);

// To maintain separation of metrics collection from NCS library code, we need a way to
// know when a fix request is made, and when the result of the request is ready (fix or error).
// There is a location event handler that indicates a location request result, but only one,
// which should be reserved for application code. There is also no callback mechanism to
// be notified when a fix request is made. Therefore, we wrap the functions of interest:
//        - location_init() -- registers an event callback. Save the application
//              callback and register a wrapper callback.
//        - location_request() -- initiates a request. Intercept it's result, and
//              start a session if the request is successful.
int __real_location_init(location_event_handler_t event_handler);
int __wrap_location_init(location_event_handler_t event_handler)
{
	real_location_event_handler = event_handler;
	return __real_location_init(memfault_location_event_handler);
}

// Ensure the substituted function signature matches the original function
_Static_assert(__builtin_types_compatible_p(__typeof__(&location_init),
					    __typeof__(&__wrap_location_init)) &&
		       __builtin_types_compatible_p(__typeof__(&location_init),
						    __typeof__(&__real_location_init)),
	       "Error: Wrapped function does not match original location_init function signature");

// Also wrap the location request to know when to start a session
int __real_location_request(const struct location_config *config);
int __wrap_location_request(const struct location_config *config)
{
	int result = __real_location_request(config);

	if (result == 0) {
		memfault_gnss_metrics_start_fix_session();
	}
	return result;
}

// Ensure the substituted function signature matches the original function
_Static_assert(
	__builtin_types_compatible_p(__typeof__(&location_request),
				     __typeof__(&__wrap_location_request)) &&
		__builtin_types_compatible_p(__typeof__(&location_request),
					     __typeof__(&__real_location_request)),
	"Error: Wrapped function does not match original location_request function signature");

static void memfault_location_event_handler(const struct location_event_data *event_data)
{
	if (event_data->id == LOCATION_EVT_LOCATION || event_data->id == LOCATION_EVT_TIMEOUT ||
	    event_data->id == LOCATION_EVT_ERROR || event_data->id == LOCATION_EVT_RESULT_UNKNOWN) {
		memfault_gnss_metrics_stop_fix_session(event_data);
	}
	real_location_event_handler(event_data);
}

/**
 * @brief Start a GNSS fix session.
 *
 */
static void memfault_gnss_metrics_start_fix_session(void)
{
	if (session_in_progress) {
		return;
	}

	session_in_progress = true;
	LOG_DBG("Starting GNSS session");
	MEMFAULT_METRICS_SESSION_START(ncs_gnss);
	MEMFAULT_METRIC_ADD(ncs_gnss_fix_request_count, 1);
}

/**
 * @brief Stop a GNSS fix session.
 *
 * @param event_data pointer to location event data
 */
static void memfault_gnss_metrics_stop_fix_session(const struct location_event_data *event_data)
{
	if (!session_in_progress) {
		return;
	}

	uint32_t session_time_ms;
	memfault_metrics_heartbeat_timer_read(
		MEMFAULT_METRICS_KEY(MEMFAULT_METRICS_SESSION_TIMER_NAME(ncs_gnss)),
		&session_time_ms);

	switch (event_data->id) {
	case LOCATION_EVT_LOCATION:
		LOG_DBG("Stopping GNSS session, fix data acquired");

		MEMFAULT_METRIC_SET_UNSIGNED(ncs_gnss_time_to_fix_ms, session_time_ms);

		uint32_t accuracy_cm = (uint32_t)(event_data->location.accuracy * 100.0f);
		MEMFAULT_METRIC_SET_UNSIGNED(ncs_gnss_fix_accuracy_cm, accuracy_cm);
		MEMFAULT_METRIC_SET_UNSIGNED(ncs_gnss_satellites_tracked_count,
					     event_data->location.details.gnss.satellites_tracked);
		break;
	case LOCATION_EVT_TIMEOUT:
		LOG_DBG("Stopping GNSS session, timeout recorded");
		MEMFAULT_METRIC_SET_UNSIGNED(ncs_gnss_search_timeout_ms, session_time_ms);
		MEMFAULT_METRIC_SET_UNSIGNED(ncs_gnss_satellites_tracked_count,
					     event_data->location.details.gnss.satellites_tracked);
		break;
	case LOCATION_EVT_RESULT_UNKNOWN:
		// This location result will occur when a timeout occurs, but another method is
		// attempted and fails (some "external" method via A-GNSS, P-GPS, LTE neighbor
		// cell and Wi-Fi access point data). Record as a timeout as well but mark with
		// a different log message for debugging purposes.
		LOG_DBG("Stopping GNSS session, timeout recorded along with unknown result");
		MEMFAULT_METRIC_SET_UNSIGNED(ncs_gnss_search_timeout_ms, session_time_ms);
		MEMFAULT_METRIC_SET_UNSIGNED(ncs_gnss_satellites_tracked_count,
					     event_data->location.details.gnss.satellites_tracked);
		break;
	case LOCATION_EVT_ERROR:
		LOG_DBG("Stopping GNSS session, error event occurred, id=%d", event_data->id);
		break;
	default:
		LOG_DBG("Stopping GNSS session, unexpected event occured: id=%d", event_data->id);
		break;
	}

	MEMFAULT_METRICS_SESSION_END(ncs_gnss);
	session_in_progress = false;
}
