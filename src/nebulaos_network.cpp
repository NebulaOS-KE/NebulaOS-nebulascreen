#include "nebulaos_network.h"

#include <arpa/inet.h>

const char *const NETWORK_STATUS_METHOD = "machine.nebulaos.network.status";
const char *const NETWORK_IPV4_METHOD = "machine.nebulaos.network.ipv4";

static bool parse_ipv4(const std::string &s, uint32_t &out) {
  struct in_addr addr;
  if (inet_pton(AF_INET, s.c_str(), &addr) != 1) return false;
  out = ntohl(addr.s_addr);
  return true;
}

int netmask_to_prefix(const std::string &netmask) {
  uint32_t m;
  if (!parse_ipv4(netmask, m)) return -1;
  uint32_t inv = ~m;
  if ((inv & (inv + 1)) != 0) return -1;
  int prefix = 0;
  while (prefix < 32 && (m & (0x80000000u >> prefix))) prefix++;
  return prefix;
}

std::string prefix_to_netmask(int prefix) {
  if (prefix < 0 || prefix > 32) return "";
  uint32_t m = prefix == 0 ? 0 : (0xFFFFFFFFu << (32 - prefix));
  return std::to_string((m >> 24) & 0xFF) + "." + std::to_string((m >> 16) & 0xFF) + "." +
         std::to_string((m >> 8) & 0xFF) + "." + std::to_string(m & 0xFF);
}

std::string validate_static_entry(const std::string &ip_s, const std::string &netmask_s,
                                  const std::string &gateway_s) {
  uint32_t ip, netmask, gateway;
  if (!parse_ipv4(ip_s, ip)) return "Invalid IP address";
  int prefix = netmask_to_prefix(netmask_s);
  if (prefix < 0) return "Netmask is not a valid contiguous mask";
  if (prefix < 8 || prefix > 30) return "Netmask must be between 255.0.0.0 and 255.255.255.252";
  parse_ipv4(netmask_s, netmask);
  if (!parse_ipv4(gateway_s, gateway)) return "Invalid gateway address";

  uint32_t network = ip & netmask;
  uint32_t broadcast = network | ~netmask;
  if ((gateway & netmask) != network) return "Gateway " + gateway_s + " is not within the " + netmask_s + " subnet";
  if (ip == gateway) return ip_s + " is the gateway address";
  if (ip == network) return ip_s + " is the network address";
  if (ip == broadcast) return ip_s + " is the broadcast address";
  return "";
}

static std::string str_field(const json &j, const char *key) {
  if (j.is_object() && j.contains(key) && j[key].is_string()) {
    return j[key].template get<std::string>();
  }
  return "";
}

static std::string first_dns(const json &j) {
  if (j.is_object() && j.contains("dns") && j["dns"].is_array() && !j["dns"].empty()
      && j["dns"][0].is_string()) {
    return j["dns"][0].template get<std::string>();
  }
  return "";
}

static int int_field(const json &j, const char *key) {
  if (j.is_object() && j.contains(key) && j[key].is_number_integer()) {
    return j[key].template get<int>();
  }
  return -1;
}

static std::string error_message(const json &reply) {
  const json &e = reply["error"];
  if (e.is_object() && e.contains("message") && e["message"].is_string()) {
    return e["message"].template get<std::string>();
  }
  return e.dump();
}

NetworkView parse_network_status(const json &reply) {
  NetworkView v;
  if (!reply.is_object()) {
    v.error = "no reply";
    return v;
  }
  if (reply.contains("error")) {
    v.error = error_message(reply);
    return v;
  }
  if (!reply.contains("result") || !reply["result"].is_object()) {
    v.error = "unexpected reply";
    return v;
  }
  const json &r = reply["result"];
  json live = r.contains("live") ? r["live"] : json::object();
  json ipv4 = json::object();
  if (r.contains("config") && r["config"].is_object() && r["config"].contains("ipv4")) {
    ipv4 = r["config"]["ipv4"];
  }
  v.ok = true;
  v.live_address = str_field(live, "address");
  v.configured_static = str_field(ipv4, "mode") == "static";
  const json &src = v.configured_static ? ipv4 : live;
  v.address = str_field(src, "address");
  v.netmask = prefix_to_netmask(int_field(src, "prefix"));
  v.gateway = str_field(src, "gateway");
  v.dns = first_dns(src);
  return v;
}

json static_ipv4_params(const std::string &ip, const std::string &netmask,
                        const std::string &gateway, const std::string &dns) {
  return {{"mode", "static"}, {"address", ip}, {"prefix", netmask_to_prefix(netmask)},
          {"gateway", gateway}, {"dns", json::array({dns})}};
}

json dhcp_ipv4_params() {
  return {{"mode", "dhcp"}};
}

ApplyOutcome ipv4_apply_outcome(const json &reply, bool to_static) {
  ApplyOutcome o;
  if (reply.is_object() && reply.contains("result") && reply["result"].is_object()
      && str_field(reply["result"], "result") == "APPLIED") {
    o.success = true;
    o.message = to_static ? "Static IP applied." : "Switched to DHCP.";
    return o;
  }
  std::string err = reply.is_object() && reply.contains("error") ? error_message(reply)
                                                                 : "unexpected reply";
  if (err.rfind("GATEWAY_UNREACHABLE_REVERTED", 0) == 0) {
    o.message = "The gateway did not answer, so the previous settings were restored.";
  } else if (err.rfind("DHCP_FAILED_REVERTED", 0) == 0) {
    o.message = "No DHCP address was received, so the previous settings were restored.";
  } else if (err.find("_REVERTED") != std::string::npos) {
    o.message = "The new settings did not work, so the previous settings were restored.";
  } else {
    o.message = "Failed to apply: " + err;
  }
  return o;
}
