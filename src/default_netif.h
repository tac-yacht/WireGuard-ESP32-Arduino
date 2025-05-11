#ifndef _DEFAULT_NETIF_H_
#define _DEFAULT_NETIF_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/netif.h"

struct netif* get_default_netif();

#endif /* _DEFAULT_NETIF_H_ */

#ifdef __cplusplus
}
#endif
