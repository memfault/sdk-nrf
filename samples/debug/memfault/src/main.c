/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <memfault/metrics/metrics.h>
#include <memfault/ports/zephyr/http.h>
#include <memfault/core/data_packetizer.h>
#include <memfault/core/trace_event.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_REGISTER(memfault_sample, CONFIG_MEMFAULT_SAMPLE_LOG_LEVEL);

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK	      (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

static K_SEM_DEFINE(nw_connected_sem, 0, 1);

/* Zephyr NET management event callback structures. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

/* Recursive Fibonacci calculation used to trigger stack overflow. */
static int fib(int n)
{
	if (n <= 1) {
		return n;
	}

	return fib(n - 1) + fib(n - 2);
}

/* Handle button presses and trigger faults that can be captured and sent to
 * the Memfault cloud for inspection after rebooting:
 * Only button 1 is available on Thingy:91, the rest are available on nRF9160 DK.
 *	Button 1: Trigger stack overflow.
 *	Button 2: Trigger NULL-pointer dereference.
 *	Switch 1: Increment switch_1_toggle_count metric by one.
 *	Switch 2: Trace switch_2_toggled event, along with switch state.
 */
static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	uint32_t buttons_pressed = has_changed & button_states;

	if (buttons_pressed & DK_BTN1_MSK) {
		LOG_WRN("Stack overflow will now be triggered");
		fib(10000);
	} else if (buttons_pressed & DK_BTN2_MSK) {
		volatile uint32_t i;

		LOG_WRN("Division by zero will now be triggered");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiv-by-zero"
		i = 1 / 0;
#pragma GCC diagnostic pop
		ARG_UNUSED(i);
	} else if (has_changed & DK_BTN3_MSK) {
		/* DK_BTN3_MSK is Switch 1 on nRF9160 DK. */
		int err = MEMFAULT_METRIC_ADD(switch_1_toggle_count, 1);
		if (err) {
			LOG_ERR("Failed to increment switch_1_toggle_count");
		} else {
			LOG_INF("switch_1_toggle_count incremented");
		}
	} else if (has_changed & DK_BTN4_MSK) {
		/* DK_BTN4_MSK is Switch 2 on nRF9160 DK. */
		MEMFAULT_TRACE_EVENT_WITH_LOG(switch_2_toggled, "Switch state: %d",
					      buttons_pressed & DK_BTN4_MSK ? 1 : 0);
		LOG_INF("switch_2_toggled event has been traced, button state: %d",
			buttons_pressed & DK_BTN4_MSK ? 1 : 0);
	}
}

static void on_connect(void)
{
#if IS_ENABLED(MEMFAULT_NCS_LTE_METRICS)
	uint32_t time_to_lte_connection;

	/* Retrieve the LTE time to connect metric. */
	memfault_metrics_heartbeat_timer_read(MEMFAULT_METRICS_KEY(ncs_lte_time_to_connect_ms),
					      &time_to_lte_connection);

	LOG_INF("Time to connect: %d ms", time_to_lte_connection);
#endif /* IS_ENABLED(MEMFAULT_NCS_LTE_METRICS) */

	LOG_INF("Sending already captured data to Memfault");

	/* Trigger collection of heartbeat data. */
	memfault_metrics_heartbeat_debug_trigger();

	/* Check if there is any data available to be sent. */
	if (!memfault_packetizer_data_available()) {
		LOG_DBG("There was no data to be sent");
		return;
	}

	LOG_DBG("Sending stored data...");

	/* Send the data that has been captured to the memfault cloud.
	 * This will also happen periodically, with an interval that can be configured using
	 * CONFIG_MEMFAULT_HTTP_PERIODIC_UPLOAD_INTERVAL_SECS.
	 */
	memfault_zephyr_port_post_data();
}

static void l4_event_handler(struct net_mgmt_event_callback *cb, uint32_t event,
			     struct net_if *iface)
{
	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("Network connectivity established");
		k_sem_give(&nw_connected_sem);
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("Network connectivity lost");
		break;
	default:
		LOG_DBG("Unknown event: 0x%08X", event);
		return;
	}
}

static void connectivity_event_handler(struct net_mgmt_event_callback *cb, uint32_t event,
				       struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
		__ASSERT(false, "Failed to connect to a network");
		return;
	}
}

static const char cert[] = {
	// // memfault g2 cert
	// "-----BEGIN CERTIFICATE-----\n"
	// "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
	// "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
	// "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
	// "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
	// "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
	// "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
	// "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
	// "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
	// "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
	// "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
	// "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
	// "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
	// "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
	// "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
	// "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
	// "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
	// "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
	// "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
	// "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
	// "MrY=\n"
	// "-----END CERTIFICATE-----\n"

	// memfault self-signed ecdsa root
	"-----BEGIN CERTIFICATE-----\n"
	"MIICczCCAfigAwIBAgIUKbNJGb1rWF1kSBv+dNIXnFHwQY0wCgYIKoZIzj0EAwMw\n"
	"cDELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDVNh\n"
	"biBGcmFuY2lzY28xFzAVBgNVBAoMDk1lbWZhdWx0LCBJbmMuMRswGQYDVQQDDBJF\n"
	"Q0RTQSBUZXN0IFJvb3QgQ0EwHhcNMjUwMjA1MTQ0MTA3WhcNMzUwMjAzMTQ0MTA3\n"
	"WjBwMQswCQYDVQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwN\n"
	"U2FuIEZyYW5jaXNjbzEXMBUGA1UECgwOTWVtZmF1bHQsIEluYy4xGzAZBgNVBAMM\n"
	"EkVDRFNBIFRlc3QgUm9vdCBDQTB2MBAGByqGSM49AgEGBSuBBAAiA2IABLHDgHoq\n"
	"vEhPNE+P4gs3j/GsBsJ6n/uXIN+4fQmNvBiHO9pRhrYJHNsnek8mL4LoHRSkvT7t\n"
	"cG1+88+oSLJL4CrmSw1UzXxGUM4GFBCbYic6CBN+DhWeSyzYa5FRdCsxOqNTMFEw\n"
	"HQYDVR0OBBYEFGjzgnfHwMRt9+h8m5Kl7soXiWBhMB8GA1UdIwQYMBaAFGjzgnfH\n"
	"wMRt9+h8m5Kl7soXiWBhMA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0EAwMDaQAw\n"
	"ZgIxAIeJhxa6SAbIMgUyxVJfbRJoSEsTabdRu8UKpaZEdCNFH+smUIf4L/9up6Pn\n"
	"7A+IjQIxAMRnSurxzIkr5LQNk/FWilw+WkUdUeU9spjocCTqP6nG2MTYOqxks4a1\n"
	"1QtV37cwuw==\n"
	"-----END CERTIFICATE-----\n"

	/* Null terminate certificate if running Mbed TLS on the application core.
	 * Required by TLS credentials API.
	 */
	IF_ENABLED(CONFIG_TLS_CREDENTIALS, (0x00))
};

BUILD_ASSERT(sizeof(cert) < KB(4), "Certificate too large");

#include <modem/modem_key_mgmt.h>

#define TLS_SEC_TAG		42

/* Provision certificate to modem */
int cert_provision(void)
{
	int err;

	LOG_INF("Provisioning certificate");

#if CONFIG_MODEM_KEY_MGMT
	bool exists;
	int mismatch;

	/* It may be sufficient for you application to check whether the correct
	 * certificate is provisioned with a given tag directly using modem_key_mgmt_cmp().
	 * Here, for the sake of the completeness, we check that a certificate exists
	 * before comparing it with what we expect it to be.
	 */
	err = modem_key_mgmt_exists(TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, &exists);
	if (err) {
		LOG_ERR("Failed to check for certificates err %d", err);
		return err;
	}

	if (exists) {
		mismatch = modem_key_mgmt_cmp(TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert,
					      sizeof(cert) - 1);
		if (!mismatch) {
			LOG_INF("Certificate match");
			return 0;
		}

		LOG_INF("Certificate mismatch");
		err = modem_key_mgmt_delete(TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
		if (err) {
			LOG_ERR("Failed to delete existing certificate, err %d", err);
		}
	}

	LOG_INF("Provisioning certificate to the modem");

	/*  Provision certificate to the modem */
	err = modem_key_mgmt_write(TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert,
				   sizeof(cert));
	if (err) {
		LOG_ERR("Failed to provision certificate, err %d", err);
		return err;
	}
#else /* CONFIG_MODEM_KEY_MGMT */
	#error not supported
#endif /* !CONFIG_MODEM_KEY_MGMT */

	return 0;
}

#include <modem/nrf_modem_lib.h>

// Cert provisioning has to happen after modem lib initialization
NRF_MODEM_LIB_ON_INIT(provision_certs, on_modem_lib_init, NULL);

static void on_modem_lib_init(int ret, void *ctx) {
	if (ret != 0) {
		/* Return if modem initialization failed */
		return;
	}

	int err = cert_provision();
	if (err) {
		LOG_ERR("Failed to provision certificates, error: %d", err);
	}
}

#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#define CONFIG_HTTPS_HOSTNAME "ecdsa-test.memfault.com"
#define HTTPS_PORT "443"

/* Setup TLS options on a given socket */
int tls_setup(int fd)
{
	int err;
	int verify;

	/* Security tag that we have provisioned the certificate with */
	const sec_tag_t tls_sec_tag[] = {
		TLS_SEC_TAG,
	};

	/* Set up TLS peer verification */
	enum {
		NONE = 0,
		OPTIONAL = 1,
		REQUIRED = 2,
	};

	verify = REQUIRED;

	err = setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
	if (err) {
		LOG_ERR("Failed to setup peer verification, err %d", errno);
		return err;
	}

	/* Associate the socket with the security tag
	 * we have provisioned the certificate with.
	 */
	err = setsockopt(fd, SOL_TLS, TLS_SEC_TAG_LIST, tls_sec_tag, sizeof(tls_sec_tag));
	if (err) {
		LOG_ERR("Failed to setup TLS sec tag, err %d", errno);
		return err;
	}

	err = setsockopt(fd, SOL_TLS, TLS_HOSTNAME,
			CONFIG_HTTPS_HOSTNAME,
			sizeof(CONFIG_HTTPS_HOSTNAME) - 1);
	if (err) {
		LOG_ERR("Failed to setup TLS hostname, err %d", errno);
		return err;
	}
	return 0;
}

static void send_http_request(void)
{
	int err;
	int fd;
	struct addrinfo *res;
	struct addrinfo hints = {
		.ai_flags = AI_NUMERICSERV, /* Let getaddrinfo() set port */
		.ai_socktype = SOCK_STREAM,
	};
	char peer_addr[INET6_ADDRSTRLEN];

	LOG_INF("🕵️ Looking up %s", CONFIG_HTTPS_HOSTNAME);

	err = getaddrinfo(CONFIG_HTTPS_HOSTNAME, HTTPS_PORT, &hints, &res);
	if (err) {
		LOG_ERR("getaddrinfo() failed, err %d", errno);
		return;
	}

	inet_ntop(res->ai_family, &((struct sockaddr_in *)(res->ai_addr))->sin_addr, peer_addr,
		  INET6_ADDRSTRLEN);
	LOG_INF("✅ Resolved %s (%s)", peer_addr, net_family2str(res->ai_family));

	if (IS_ENABLED(CONFIG_SAMPLE_TFM_MBEDTLS)) {
		fd = socket(res->ai_family, SOCK_STREAM | SOCK_NATIVE_TLS, IPPROTO_TLS_1_2);
	} else {
		fd = socket(res->ai_family, SOCK_STREAM, IPPROTO_TLS_1_2);
	}
	if (fd == -1) {
		LOG_ERR("Failed to open socket!");
		goto clean_up;
	}

	/* Setup TLS socket options */
	err = tls_setup(fd);
	if (err) {
		goto clean_up;
	}

	LOG_INF("🔌 Connecting to %s:%d", CONFIG_HTTPS_HOSTNAME,
	       ntohs(((struct sockaddr_in *)(res->ai_addr))->sin_port));
	err = connect(fd, res->ai_addr, res->ai_addrlen);
	if (err) {
		LOG_ERR("connect() failed, err: %d", errno);
		goto clean_up;
	}

	LOG_INF("✅ Connection succeeded");

	clean_up:
	freeaddrinfo(res);
	(void)close(fd);
}


int main(void)
{
	int err;

	// 2 second delay so logs aren't eaten
	k_sleep(K_SECONDS(2));

	LOG_INF("Memfault sample has started");

	err = dk_buttons_init(button_handler);
	if (err) {
		LOG_ERR("dk_buttons_init, error: %d", err);
	}

	/* Setup handler for Zephyr NET Connection Manager events. */
	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	/* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);

	/* Connecting to the configured connectivity layer.
	 * Wi-Fi or LTE depending on the board that the sample was built for.
	 */
	LOG_INF("Bringing network interface up and connecting to the network");

	err = conn_mgr_all_if_up(true);
	if (err) {
		__ASSERT(false, "conn_mgr_all_if_up, error: %d", err);
		return err;
	}

	err = conn_mgr_all_if_connect(true);
	if (err) {
		__ASSERT(false, "conn_mgr_all_if_connect, error: %d", err);
		return err;
	}

	/* Performing in an infinite loop to be resilient against
	 * re-connect bursts directly after boot, e.g. when connected
	 * to a roaming network or via weak signal. Note that
	 * Memfault data will be uploaded periodically every
	 * CONFIG_MEMFAULT_HTTP_PERIODIC_UPLOAD_INTERVAL_SECS.
	 * We post data here so as soon as a connection is available
	 * the latest data will be pushed to Memfault.
	 */

	while (1) {
		k_sem_take(&nw_connected_sem, K_FOREVER);
		LOG_INF("Connected to network");
		send_http_request();
		on_connect();
	}
}
