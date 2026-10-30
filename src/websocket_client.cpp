/*
 * websocket client
 *
 * @build   make examples
 * @server  bin/websocket_server_test 8888
 * @client  bin/websocket_client_test ws://127.0.0.1:8888/test
 * @clients bin/websocket_client_test ws://127.0.0.1:8888/test 100
 * @python  scripts/websocket_server.py
 * @js      html/websocket_client.html
 *
 */

#include "websocket_client.h"
#include "spdlog/spdlog.h"

#include <algorithm>

using namespace hv;
using json = nlohmann::json;

KWebSocketClient::KWebSocketClient(EventLoopPtr loop)
  : WebSocketClient(loop)
  , id(0)
{
}

KWebSocketClient::~KWebSocketClient() {
}

int KWebSocketClient::connect(const char* url,
			      std::function<void()> connected,
			      std::function<void()> disconnected) {
  spdlog::debug("websocket connecting");
  // set callbacks
  onopen = [this, connected]() {
    const HttpResponsePtr& resp = getHttpResponse();
    spdlog::debug("onopen {}", resp->body.c_str());
    connected();
  };
  onmessage = [this, connected, disconnected](const std::string &msg) {
    // if (msg.find("notify_proc_stat_update") == std::string::npos) {
    //   spdlog::trace("onmessage(type={} len={}): {}", opcode() == WS_OPCODE_TEXT ? "text" : "binary",
    // 	     (int)msg.size(), msg);
    // }
    auto j = json::parse(msg);

    NotifyConsumer *consumer = nullptr;
    std::function<void(json&)> cb;
    std::vector<NotifyConsumer*> notify;
    std::vector<std::function<void(json&)>> handlers;
    std::string method;
    if (j.contains("method")) {
      method = j["method"].template get<std::string>();
    }
    {
      std::lock_guard<std::mutex> guard(maps_lock);
      if (j.contains("id") && j["id"].is_number_unsigned()) {
        uint32_t rid = j["id"].template get<uint32_t>();
        const auto &entry = consumers.find(rid);
        if (entry != consumers.end()) {
          consumer = entry->second;
          consumers.erase(entry);
        }
        const auto &cb_entry = callbacks.find(rid);
        if (cb_entry != callbacks.end()) {
          cb = std::move(cb_entry->second);
          callbacks.erase(cb_entry);
        }
      }
      if (!method.empty()) {
        if ("notify_status_update" == method || "notify_filelist_changed" == method) {
          notify = notify_consumers;
        }
        const auto &entry = method_resp_cbs.find(method);
        if (entry != method_resp_cbs.end()) {
          for (const auto &handler_entry : entry->second) {
            handlers.push_back(handler_entry.second);
          }
        }
      }
    }

    if (consumer != nullptr) {
      consumer->consume(j);
    }
    if (cb) {
      cb(j);
    }

    if (!method.empty()) {
      for (const auto &entry : notify) {
        entry->consume(j);
      }
      //  else if ("notify_gcode_response" == method) {
      // 	for (const auto &gcode_cb : gcode_resp_cbs) {
      // 	  gcode_cb(j);
      // 	}
      // }
      if ("notify_klippy_disconnected" == method) {
        disconnected();
      } else if ("notify_klippy_ready" == method) {
        connected();
      }

      for (const auto &handler : handlers) {
        handler(j);
      }
    }
  };

  onclose = [disconnected]() {
    spdlog::debug("onclose");
    disconnected();
  };

  // ping
  setPingInterval(10000);

  reconn_setting_t reconn;
  reconn_setting_init(&reconn);
  reconn.min_delay = 200;
  reconn.max_delay = 2000;
  reconn.delay_policy = 2;
  setReconnect(&reconn);

  http_headers headers;
  return open(url, headers);
};

int KWebSocketClient::send_jsonrpc(const std::string &method,
				   const json &params,
				   std::function<void(json&)> cb) {
  uint64_t rid = id++;
  {
    std::lock_guard<std::mutex> guard(maps_lock);
    callbacks[rid] = cb;
  }
  // XXX: check success, remove the callback if the send is unsuccessful
  return send_rpc(method, &params, rid);
}

int KWebSocketClient::send_jsonrpc(const std::string &method,
				   std::function<void(json&)> cb) {
  uint64_t rid = id++;
  {
    std::lock_guard<std::mutex> guard(maps_lock);
    callbacks[rid] = cb;
  }
  return send_rpc(method, nullptr, rid);
}

int KWebSocketClient::send_jsonrpc(const std::string &method, const json &params, NotifyConsumer *consumer) {
  uint64_t rid = id++;
  {
    std::lock_guard<std::mutex> guard(maps_lock);
    consumers[rid] = consumer;
  }
  return send_rpc(method, &params, rid);
}

void KWebSocketClient::register_notify_update(NotifyConsumer *consumer) {
  std::lock_guard<std::mutex> guard(maps_lock);
  if (std::find(notify_consumers.begin(), notify_consumers.end(), consumer) == std::end(notify_consumers)) {
    notify_consumers.push_back(consumer);
  }
}

void KWebSocketClient::unregister_notify_update(NotifyConsumer *consumer) {
  std::lock_guard<std::mutex> guard(maps_lock);
  notify_consumers.erase(std::remove_if(
    notify_consumers.begin(), notify_consumers.end(),
    [consumer](NotifyConsumer *c) {
      return c == consumer;
    }), notify_consumers.end());
}

int KWebSocketClient::send_rpc(const std::string &method, const json *params, uint64_t rid) {
  json rpc;
  rpc["jsonrpc"] = "2.0";
  rpc["method"] = method;
  if (params != nullptr) {
    rpc["params"] = *params;
  }
  rpc["id"] = rid;

  spdlog::debug("send_jsonrpc: {}", rpc.dump());
  return send(rpc.dump());
}

int KWebSocketClient::send_jsonrpc(const std::string &method,
				   const json &params) {
  return send_rpc(method, &params, id++);
}

int KWebSocketClient::send_jsonrpc(const std::string &method) {
  return send_rpc(method, nullptr, id++);
}

int KWebSocketClient::gcode_script(const std::string &gcode) {
  json cmd = {{ "script", gcode }};
  return send_jsonrpc("printer.gcode.script", cmd);
}

int KWebSocketClient::gcode_script(const std::string &gcode, std::function<void(json&)> cb) {
  json cmd = {{ "script", gcode }};
  return send_jsonrpc("printer.gcode.script", cmd, cb);
}

void KWebSocketClient::register_method_callback(std::string resp_method,
						std::string handler_name,
						std::function<void(json&)> cb) {
  std::lock_guard<std::mutex> guard(maps_lock);
  const auto &entry = method_resp_cbs.find(resp_method);
  if (entry == method_resp_cbs.end()) {
    spdlog::debug("registering new method {}, handler {}", resp_method, handler_name);
    std::map<std::string, std::function<void(json&)>> handler_map;
    handler_map.insert({handler_name, cb});
    method_resp_cbs.insert({resp_method, handler_map});
  } else {
    spdlog::debug("found existing resp_method {} with handlers, updating handler callback {}",
		  resp_method, handler_name);
    entry->second.insert({handler_name, cb});
  }
}
