/*
 * WireGuard implementation for ESP32 Arduino by Kenta Ida (fuga@fugafuga.org)
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "WireGuard-ESP32.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip.h"
#include "lwip/netdb.h"

#include "esp_log.h"

extern "C" {
#include "wireguardif.h"
#include "wireguard-platform.h"
}

// Wireguard instance
static struct netif wg_netif_struct = {0};
static struct netif *wg_netif = NULL;
static struct netif *previous_default_netif = NULL;
static uint8_t wireguard_peer_index = WIREGUARDIF_INVALID_INDEX;

#define TAG "[WireGuard] "

#ifdef FOR_ARDUINO
bool WireGuard::begin(const IPAddress& localIP, const IPAddress& Subnet, const IPAddress& Gateway, const char* privateKey, const char* remotePeerAddress, const char* remotePeerPublicKey, uint16_t remotePeerPort) {
	auto _localIP = IPADDR4_INIT(static_cast<uint32_t>(localIP));
	auto _Subnet = IPADDR4_INIT(static_cast<uint32_t>(Subnet));
	auto _Gateway = IPADDR4_INIT(static_cast<uint32_t>(Gateway));
	return WireGuard::begin(_localIP, _Subnet, _Gateway, privateKey, remotePeerAddress, remotePeerPublicKey, remotePeerPort);
}
#endif

bool WireGuard::begin(const ip_addr_t& localIP, const ip_addr_t& Subnet, const ip_addr_t& Gateway, const char* privateKey, const char* remotePeerAddress, const char* remotePeerPublicKey, uint16_t remotePeerPort) {
	struct wireguardif_init_data wg;
	struct wireguardif_peer peer;

	assert(privateKey != NULL);
	assert(remotePeerAddress != NULL);
	assert(remotePeerPublicKey != NULL);
	assert(remotePeerPort != 0);

	// Setup the WireGuard device structure
	wg.private_key = privateKey;
    wg.listen_port = remotePeerPort;
	
	wg.bind_netif = NULL;

	// Initialise the first WireGuard peer structure
	wireguardif_peer_init(&peer);
	// If we know the endpoint's address can add here
	bool success_get_endpoint_ip = false;
    for(int retry = 0; retry < 5; retry++) {
        ip_addr_t endpoint_ip = IPADDR4_INIT_BYTES(0, 0, 0, 0);
        struct addrinfo *res = NULL;
        struct addrinfo hint;
        memset(&hint, 0, sizeof(hint));
        memset(&endpoint_ip, 0, sizeof(endpoint_ip));
        if( lwip_getaddrinfo(remotePeerAddress, NULL, &hint, &res) != 0 ) {
			vTaskDelay(pdMS_TO_TICKS(2000));
			continue;
		}
		success_get_endpoint_ip = true;
        struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
        inet_addr_to_ip4addr(ip_2_ip4(&endpoint_ip), &addr4);
        lwip_freeaddrinfo(res);

        peer.endpoint_ip = endpoint_ip;
        ESP_LOGI(TAG, "%s is %s", remotePeerAddress, ipaddr_ntoa(&endpoint_ip));
		break;
    }
	if( !success_get_endpoint_ip  ) {
		ESP_LOGE(TAG, "failed to get endpoint ip.");
		return false;
	}
	// Register the new WireGuard network interface with lwIP
	wg_netif = netif_add(&wg_netif_struct, ip_2_ip4(&localIP), ip_2_ip4(&Subnet), ip_2_ip4(&Gateway), &wg, &wireguardif_init, &ip_input);
	if( wg_netif == nullptr ) {
		ESP_LOGE(TAG, "failed to initialize WG netif.");
		return false;
	}
	// Mark the interface as administratively up, link up flag is set automatically when peer connects
	netif_set_up(wg_netif);

	peer.public_key = remotePeerPublicKey;
	peer.preshared_key = NULL;
	// Allow all IPs through tunnel
    {
        ip_addr_t allowed_ip = IPADDR4_INIT_BYTES(0, 0, 0, 0);
        peer.allowed_ip = allowed_ip;
        ip_addr_t allowed_mask = IPADDR4_INIT_BYTES(0, 0, 0, 0);
        peer.allowed_mask = allowed_mask;
    }
	
	peer.endport_port = remotePeerPort;

    // Initialize the platform
    wireguard_platform_init();
	// Register the new WireGuard peer with the netwok interface
	wireguardif_add_peer(wg_netif, &peer, &wireguard_peer_index);
	if ((wireguard_peer_index != WIREGUARDIF_INVALID_INDEX) && !ip_addr_isany(&peer.endpoint_ip)) {
		// Start outbound connection to peer
        ESP_LOGI(TAG, "connecting wireguard...");
		wireguardif_connect(wg_netif, wireguard_peer_index);
		// Save the current default interface for restoring when shutting down the WG interface.
		previous_default_netif = netif_default;
		// Set default interface to WG device.
        netif_set_default(wg_netif);
	}

	this->_is_initialized = true;
	return true;
}

#ifdef FOR_ARDUINO
bool WireGuard::begin(const IPAddress& localIP, const char* privateKey, const char* remotePeerAddress, const char* remotePeerPublicKey, uint16_t remotePeerPort) {
	// Maintain compatiblity with old begin 
	auto subnet = IPAddress(255,255,255,255);
	auto gateway = IPAddress(0,0,0,0);
	return WireGuard::begin(localIP, subnet, gateway, privateKey, remotePeerAddress, remotePeerPublicKey, remotePeerPort);
}
#endif

void WireGuard::end() {
	if( !this->_is_initialized ) return;

	// Restore the default interface.
	netif_set_default(previous_default_netif);
	previous_default_netif = nullptr;
	// Disconnect the WG interface.
	wireguardif_disconnect(wg_netif, wireguard_peer_index);
	// Remove peer from the WG interface
	wireguardif_remove_peer(wg_netif, wireguard_peer_index);
	wireguard_peer_index = WIREGUARDIF_INVALID_INDEX;
	// Shutdown the wireguard interface.
	wireguardif_shutdown(wg_netif);
	// Remove the WG interface;
	netif_remove(wg_netif);
	wg_netif = nullptr;

	this->_is_initialized = false;
}