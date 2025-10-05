#include <Windows.h>
#include <iostream>
#include <future>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

#include "executor/websocket.h"
#include "executor/instance.h"
#include "executor/utils.h"
#include "executor/client.h"

Websocket::Websocket(std::string host, int port) : _host(host), _port(port), _server(port, host) {
    _server.setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> connection_state, ix::WebSocket& websocket, const ix::WebSocketMessagePtr& message) {
            if (message->type == ix::WebSocketMessageType::Open) {
                // nothing to do with this yet
            }
            else if (message->type == ix::WebSocketMessageType::Close) {
                std::lock_guard<std::mutex> lock(_mutex);

                auto it = std::find_if(
                    _connections.begin(),
                    _connections.end(),
                    [&](const std::pair<DWORD, std::shared_ptr<ix::WebSocket>>& pair)
                    {
                        return pair.second.get() == &websocket;
                    });

                if (it != _connections.end())
                {
                    REPLPrint("[*] Client " + std::to_string(it->first) + " disconnected");
                    _connections.erase(it);
                }
            } else if (message->type == ix::WebSocketMessageType::Message) {
                json data = json::parse(message->str, nullptr, false);

                if (data.is_discarded()) return;

                std::string id = data.contains("id") ? data["id"] : "";
                std::string type = data.contains("type") ? data["type"] : "";

                

                if (!id.empty() and type == "response") {
                    auto it = _on_going_requests.find(id);
                    if (it != _on_going_requests.end())
                    {
                        it->second->set_value(data);
                        _on_going_requests.erase(it);
                        return;
                    }
                }

                if (!data.contains("action") || !data.contains("pid")) return;

                std::string action = data["action"];
                DWORD PID = (DWORD)data["pid"];


                if (action == "initialize") {
                    for (const auto& connection : _server.getClients()) {
                        if (connection.get() == &websocket) {
                            REPLPrint("[*] Client " + std::to_string(PID) + " connected");

                            _connections.emplace((DWORD)data["pid"], connection);
                            websocket.setPingInterval(10000);
                        }
                    }
                }
                else if (action == "is_compilable") {
                    json response;

                    response["type"] = "response";
                    if (!id.empty()) response["id"] = id;


                    if (!data.contains("source")) {
                        response["success"] = false;
                        response["message"] = "source not found";
                        return Send(response, PID);
                    }
                        
                    Result compile_result = Compile(data["source"]);
                    response["success"] = compile_result.success;

                    if (!compile_result.success) response["message"] = compile_result.message;

                    return Send(response, PID);
                }
                else if (action == "loadstring") {
                    json response;

                    response["type"] = "response";
                    response["success"] = false;
                    if (!id.empty()) response["id"] = id;

                    if (!data.contains("chunk")) {
                        response["message"] = "source not found";
                        return Send(response, PID);
                    }
                    else if (!data.contains("chunk_name")) {
                        response["message"] = "script_name not found";
                        return Send(response, PID);
                    }
                    else if (!data.contains("script_name")) {
                        response["message"] = "script_name not found";
                        return Send(response, PID);
                    }

                    Result compile_result = Compile(data["chunk"]);

                    if (!compile_result.success) {
                        response["message"] = compile_result.message;
                        return Send(response, PID);
                    }

                    for (const auto& client : _clients) {
                        if (client->GetProcessId() == PID) {

                            Result loadstring_result = client->Loadstring(data["chunk"], data["chunk_name"], data["script_name"]);
                            response["success"] = loadstring_result.success;

                            if (!loadstring_result.success) response["message"] = loadstring_result.message;
                            
                            return Send(response, PID);
                        }
                    }

                }
            }
        }
    );
}

Websocket::~Websocket() {
    Stop();
}

bool Websocket::Run() {
    auto res = _server.listen();
    if (!res.first)
    {
        std::cerr << "Failed to listen: " << res.second << std::endl;
        return false;
    }

    _server.start();
    std::cout << "Server running at ws://" << _host << ":" << _port << std::endl;
    return true;
}

void Websocket::Stop() {
    _server.stop();
    std::cout << "Server stopped." << std::endl;

    std::lock_guard<std::mutex> lock(_mutex);
    _connections.clear();

}


void Websocket::Send(std::string data, DWORD PID) const {
    auto connection = GetConnection(PID);
    if (!connection) return;

    connection->send(data);
}

void Websocket::Send(const json& data, DWORD PID) const {
    try {
        std::string encoded = data.dump();
        Send(encoded, PID);
    } catch (const json::exception& exeception) {
        return;
    }

}

json Websocket::SendAndReceive(json& data, DWORD PID, int timeout) {
    std::string GUID = GenerateGUID();

    data["id"] = GUID;

    auto promise = std::make_unique<std::promise<json>>();
    auto future = promise->get_future();

    _on_going_requests.emplace(GUID, std::move(promise));


    Send(data, PID);

    if (future.wait_for(std::chrono::seconds(timeout)) == std::future_status::ready)
    {

        json response = future.get();

        _on_going_requests.erase(GUID);

        return response;
    }
    else
    {
        _on_going_requests.erase(GUID);

        throw std::runtime_error("timeout waiting for response");
    }
}

int Websocket::GetPort() const {
	return _port;
}

const Clients& Websocket::GetClients() const {
    return _clients;
}

const Connections& Websocket::GetConnections() const {
    return _connections;
}


std::shared_ptr<ix::WebSocket> Websocket::GetConnection(DWORD PID) const {
    auto it = _connections.find(PID);

    if (it != _connections.end()) return it->second;

    return nullptr;
}

void Websocket::AddClient(std::unique_ptr<Client> client) {
    _clients.push_back(std::move(client));
}