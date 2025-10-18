#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>

#include "executor/utils.h"
#include "executor/process.h"


class Websocket;

class Client {
private:
	Process _process;
	Websocket* _server;

	 static constexpr std::string_view version = "1.0.0";
public:
	Client(DWORD PID, Websocket* server);

	const Process* GetProcess() const;
	std::string GetInitScript() const;

	void Inject() const;
	void Execute(const std::string& source) const;
};

typedef std::vector<std::unique_ptr<Client>> Clients;