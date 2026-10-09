#ifndef __NEBULAOS_NETWORK_H__
#define __NEBULAOS_NETWORK_H__

#include "hv/json.hpp"

#include <string>

using json = nlohmann::json;

// Pure, LVGL-free logic for the static IP panel over the NebulaOS network contract
// (Moonraker component [nebulaos], NebulaOS-firmware overlay
// usr/lib/nebulaos/python/nebulaos_osconfig/network.py):
//   machine.nebulaos.network.status -> {config{hostname, ipv4{mode, address, prefix,
//                                       gateway, dns[]}}, live{address, prefix, gateway,
//                                       dns[], ssid}, boot_state, ...}
//   machine.nebulaos.network.ipv4   {"mode":"dhcp"} or {"mode":"static", "address",
//                                   "prefix", "gateway", "dns":[...]}
// The ipv4 call verifies the new setting (static: gateway reachable; DHCP: a lease) and
// reverts it automatically when it does not work; it can take up to a couple of minutes.
// The setting is persistent and global for wlan0 (not per Wi-Fi network).
//
// This file may be distributed under the terms of the GNU GPLv3 license.

extern const char *const NETWORK_STATUS_METHOD;
extern const char *const NETWORK_IPV4_METHOD;

// Dotted netmask -> prefix length; -1 when not a contiguous mask.
int netmask_to_prefix(const std::string &netmask);
// Prefix length 0..32 -> dotted netmask; "" outside that range.
std::string prefix_to_netmask(int prefix);

// Client-side check before sending (the server validates again): returns "" or a message.
std::string validate_static_entry(const std::string &ip, const std::string &netmask,
                                  const std::string &gateway);

struct NetworkView {
  bool ok = false;           // status reply usable
  std::string error;         // when !ok
  bool configured_static = false;
  // Field values: the configured static values when static, else the live ones.
  std::string address, netmask, gateway, dns;
  std::string live_address;
};

NetworkView parse_network_status(const json &reply);

json static_ipv4_params(const std::string &ip, const std::string &netmask,
                        const std::string &gateway, const std::string &dns);
json dhcp_ipv4_params();

// Outcome of an ipv4 call: success, plus a message for the status line.
struct ApplyOutcome {
  bool success = false;
  std::string message;
};
ApplyOutcome ipv4_apply_outcome(const json &reply, bool to_static);

#endif // __NEBULAOS_NETWORK_H__
