#include "default_netif.h"

#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
#define ESP_IDF
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#else
#include "tcpip_adapter.h"
#endif

netif* get_default_netif() {
#ifdef ESP_IDF
	return esp_netif_get_netif_impl(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));
	//key -> https://github.com/espressif/esp-idf/blob/v4.1/components/esp_netif/esp_netif_defaults.c#L42
#else //tcpip_adapter.h 4.1>version
    struct netif* result_netif;
	tcpip_adapter_get_netif(TCPIP_ADAPTER_IF_STA, &result_netif);
    return result_netif;
#endif

}