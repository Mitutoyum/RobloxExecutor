#include <Windows.h>
#include <iostream>
#include <future>
#include <regex>

#include "httplib/httplib.h"
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
                            _connections.emplace((DWORD)data["pid"], connection);
                            websocket.setPingInterval(10000);
                        }
                    }
                }
                else if (action == "is_compilable") {
                    json response;

                    response["type"] = "response";
                    response["success"] = false;
                    if (!id.empty()) response["id"] = id;

                    
                    try {
                        CheckRequiredKeys(data, {"source"});

                        std::string bytecode = Compile(data["source"]);
                        response["success"] = true;
                    }
                    catch (const std::exception& exception) {
                        response["message"] = exception.what();
                    }

                    return Send(response, PID);
                }
                else if (action == "request") {
                    json response;

                    response["type"] = "response";
                    response["success"] = false;
                    if (!id.empty()) response["id"] = id;

                    try {
                        CheckRequiredKeys(data, { "url", "method"});

                        const std::string& url = data["url"];
                        const std::string& method = data["method"];
                        const json headers = data.contains("headers") ? data["headers"] : json::object();
                        const std::string body = data.contains("body") ? data["body"] : "";

                        std::regex url_regex(R"(^(https?:\/\/(?:[a-zA-Z0-9-]+\.)+[a-zA-Z]{2,}|https?:\/\/(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)){3})(:\d{1,5})?(\/[^\s]*)?$)", std::regex::icase);
                        std::smatch url_match;
                        
                        if (!std::regex_match(url, url_match, url_regex))
                            throw std::runtime_error("Invalid URL");

                        std::string host = url_match[1];
                        std::string path = (url_match[3].matched ? url_match[3].str() : "/");

                        httplib::Client client(host.c_str());
                        client.set_follow_location(true);

                        httplib::Headers rHeaders;
                        for (auto it = headers.begin(); it != headers.end(); ++it) {
                            rHeaders.insert({ it.key(), it.value()});
                        }

                        httplib::Result result;

                        if (method == "GET") {
                            result = client.Get(path, rHeaders);
                        }
                        else if (method == "POST") {
                            result = client.Post(path, rHeaders, body, "application/json");
                        }
                        else if (method == "PUT") {
                            result = client.Put(path, rHeaders, body, "application/json");
                        }
                        else if (method == "DELETE") {
                            result = client.Delete(path, rHeaders, body, "application/json");
                        }
                        else if (method == "PATCH") {
                            result = client.Patch(path, rHeaders, body, "application/json");
                        }
                        else throw std::runtime_error("Unsupported HTTP Method");

                        if (result) {
                            response["response"]["success"] = 200 <= result->status <= 299;
                            response["response"]["status_code"] = result->status;
                            response["response"]["status_message"] = result->reason;
                            
                            std::cout << response["response"]["success"];

                            json json_headers;

                            for (const auto& header : result->headers) {
                                json_headers[header.first] = header.second;
                            }

                            response["response"]["headers"] = json_headers;
                            response["response"]["body"] = result->body;
                        }
                        response["success"] = true;
                    }
                    catch (const std::exception& exception) {
                        response["message"] = exception.what();
                    }

                    return Send(response, PID);

                }
                else if (action == "loadstring") {
                    json response;

                    response["type"] = "response";
                    response["success"] = false;
                    if (!id.empty()) response["id"] = id;

                    try {
                        CheckRequiredKeys(data, { "chunk", "chunk_name", "script_name" });

                        const std::string& chunk = data["chunk"];
                        const std::string& chunk_name = data["chunk_name"];
                        const std::string& script_name = data["script_name"];

                        for (const auto& client : _clients) {
                            if (client->GetProcessId() == PID) {

                                uintptr_t address = client->GetAddress();
                                HANDLE handle = client->GetHandle();

                                auto Datamodel = std::make_unique<Instance>(GetDatamodel(address, handle), handle);

                                auto Script = Datamodel->FindFirstChildOfClass("RobloxReplicatedStorage")
                                    ->FindFirstChild("Executor")
                                    ->FindFirstChild("Scripts")
                                    ->FindFirstChild(script_name);

                                std::string bytecode = Compile("local function " + chunk_name + "(...)do setmetatable(getgenv and getgenv()or{},{__newindex=function(t,i,v)rawset(t,i,v)getfenv()[i]=v;end})end;" + chunk + "\nend;return " + chunk_name);

                                Script->UnlockModule();
                                Script->SetBytecode(bytecode);
                                response["success"] = true;
                            }
                        }
                    }
                    catch (const std::exception& exception) {
                        response["message"] = exception.what();
                    }

                    return Send(response, PID);
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