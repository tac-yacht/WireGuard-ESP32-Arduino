/*
 * WireGuard implementation for ESP32 Arduino by Kenta Ida (fuga@fugafuga.org)
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once
#if __has_include(<IPAddress.h>)
#include <IPAddress.h>
#define FOR_ARDUINO
#endif

class WireGuard
{
private:
    bool _is_initialized = false;
public:
    bool begin(const ip_addr_t& localIP, const ip_addr_t& Subnet, const ip_addr_t& Gateway, const char* privateKey, const char* remotePeerAddress, const char* remotePeerPublicKey, uint16_t remotePeerPort);
#ifdef FOR_ARDUINO
    bool begin(const IPAddress& localIP, const IPAddress& Subnet, const IPAddress& Gateway, const char* privateKey, const char* remotePeerAddress, const char* remotePeerPublicKey, uint16_t remotePeerPort);
    bool begin(const IPAddress& localIP, const char* privateKey, const char* remotePeerAddress, const char* remotePeerPublicKey, uint16_t remotePeerPort);
#endif
    void end();
    bool is_initialized() const { return this->_is_initialized; }
};
