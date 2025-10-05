#pragma once

#include <Windows.h>
#include <map>
#include <future>
#include <memory>
#include <unordered_map>

#include "ixwebsocket/IXWebSocketServer.h"
#include "executor/client.h"
#include "executor/repl.h"

#include "nlohmann/json.hpp"
using nlohmann::json;

typedef std::map<DWORD, std::shared_ptr<ix::WebSocket>> Connections;

class Websocket {
private:
	ix::WebSocketServer _server;
	Connections _connections;
	Clients _clients;

	std::string _host;
	int _port;

	std::unordered_map<std::string, std::unique_ptr<std::promise<json>>> _on_going_requests;
	std::mutex _mutex;

public:
	Websocket(std::string host, int port);
	~Websocket();

	bool Run();
	void Stop();

	void Send(std::string data, DWORD PID) const;
	void Send(const json& data, DWORD PID) const;

	json SendAndReceive(json& data, DWORD PID, int timeout=5);


	int GetPort() const;
	const Clients& GetClients() const;
	const Connections& GetConnections() const;
	std::shared_ptr<ix::WebSocket> GetConnection(DWORD PID) const;

	void AddClient(std::unique_ptr<Client> client);
};