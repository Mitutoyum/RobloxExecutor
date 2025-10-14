
#include <Windows.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>

#include "nlohmann/json.hpp"
using nlohmann::json;

#include "executor/client.h"
#include "executor/process.h"
#include "executor/offsets.h"
#include "executor/instance.h"
#include "executor/utils.h"
#include "executor/resource.h"
#include "executor/websocket.h"


Client::Client(DWORD PID, Websocket& server) : _PID(PID), _server(server), _handle(OpenProcess(PROCESS_ALL_ACCESS, NULL, PID)), _address(GetBaseAddress(PID)) {
	if (!_handle) {
		throw std::runtime_error("Failed to open of process " + std::to_string(PID) + ", is the PID valid?");
	}

	if (!_address) {
		throw std::runtime_error("Failed to get base address of process " + PID);
	}

}

uintptr_t Client::GetAddress() const {
	return _address;
}

HANDLE Client::GetHandle() const {
	return _handle;
}

DWORD Client::GetProcessId() const {
	return _PID;
}

std::string Client::GetInitScript() const {
	std::string initscript;

	HMODULE module = GetModule();
	if (!module)
		throw std::runtime_error("Failed to get module handle for initscript");

	HRSRC resource = FindResource(module, MAKEINTRESOURCE(INIT), MAKEINTRESOURCE(LUA));
	if (!resource)
		throw std::runtime_error("Failed to find initscript");

	HGLOBAL data = LoadResource(module, resource);
	if (!data)
		throw std::runtime_error("Failed to load initscript");

	DWORD size = SizeofResource(module, resource);
	char* final_data = static_cast<char*>(LockResource(data));

	initscript.assign(final_data, size);
	ReplaceString(initscript, "%PROCESS_ID%", std::to_string(_PID));
	ReplaceString(initscript, "%VERSION%", "\"" + version + "\"");

	return initscript;
}



void Client::Inject() const {
	auto Datamodel = std::make_unique<Instance>(GetDatamodel(_address, _handle), _handle);

	auto VRNavigation = Datamodel->FindFirstChild("StarterPlayer")
		->FindFirstChild("StarterPlayerScripts")
		->FindFirstChild("PlayerModule")
		->FindFirstChild("ControlModule")
		->FindFirstChild("VRNavigation");

	auto PlayerListManager = Datamodel->FindFirstChild("CoreGui")
		->FindFirstChild("RobloxGui")
		->FindFirstChild("Modules")
		->FindFirstChild("PlayerList")
		->FindFirstChild("PlayerListManager");

	std::string initscript = GetInitScript();
	std::string script = "script.Parent=nil;task.spawn(function()" + initscript + "\nend);while true do task.wait(9e9) end";
	std::string bytecode = Compile(script);

	VRNavigation->UnlockModule();
	VRNavigation->SetBytecode(bytecode);
	PlayerListManager->SpoofWith(VRNavigation->Self());

	FocusWindow();

	Write<uintptr_t>(_address + Offsets::FFlags::WebSocketServiceEnableClientCreation, 1, _handle); // enable websocket client creation, important for communication

	INPUT inputs[2] = {};

	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = VK_ESCAPE;
	inputs[0].ki.wScan = MapVirtualKey(VK_ESCAPE, 0);

	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = VK_ESCAPE;
	inputs[1].ki.wScan = MapVirtualKey(VK_ESCAPE, 0);
	inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

	UINT uSent = SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
	if (uSent != ARRAYSIZE(inputs))
		throw std::runtime_error("Failed to send inputs");

	Sleep(500);

	PlayerListManager->SpoofWith(PlayerListManager->Self());
}

void Client::FocusWindow() const {
	HWND client_hwnd = GetWindowFromProcessId(_PID);

	if (!client_hwnd)
		throw std::runtime_error("Failed to focus window");

	SetForegroundWindow(client_hwnd);

}

void Client::Execute(const std::string& source) const {
	json request;

	request["action"] = "execute";
	request["source"] = source;

	json response = _server.SendAndReceive(request, _PID);

	if (!response["success"])
		throw std::runtime_error(response["message"]);
}