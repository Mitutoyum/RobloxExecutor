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
public:
	Client(DWORD PID, Websocket& server);
	Result Initialize() const;

	uintptr_t GetAddress() const;
	HANDLE GetHandle() const;
	DWORD GetProcessId() const;
	Result GetInitScript() const;

	bool FocusWindow() const;

	Result Execute(const std::string& source) const;
	Result Loadstring(const std::string& chunk, const std::string& chunk_name, const std::string& script_name) const;

};

typedef std::vector<std::unique_ptr<Client>> Clients;