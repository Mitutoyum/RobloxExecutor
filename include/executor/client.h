#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>

#include "executor/utils.h"


class Websocket;

class Client {
private:
	DWORD _PID;
	HANDLE _handle;
	uintptr_t _address;
	
	Websocket& _server;
	const std::string& version = "1.0.0";
public:
	Client(DWORD PID, Websocket& server);
	void Inject() const;

	uintptr_t GetAddress() const;
	HANDLE GetHandle() const;
	DWORD GetProcessId() const;
	std::string GetInitScript() const;

	void FocusWindow() const;

	void Execute(const std::string& source) const;
};

typedef std::vector<std::unique_ptr<Client>> Clients;