
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


Client::Client(DWORD PID, Websocket* server) : _process(PID), _server(server) {}

const Process* Client::GetProcess() const {
	return &_process;
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
	ReplaceString(initscript, "%PROCESS_ID%", std::to_string(_process.GetProcessId()));
	ReplaceString(initscript, "%VERSION%", "\"" + std::string(version) + "\"");

	return initscript;
}

void Client::Inject() const {
	auto Datamodel = Instance::New(GetDatamodel(GetProcess()), GetProcess());
	auto VRNavigation = Instance::FindFirstChildFromPath(Datamodel.get(), "StarterPlayer.StarterPlayerScripts.PlayerModule.ControlModule.VRNavigation");
	auto PlayerListManager = Instance::FindFirstChildFromPath(Datamodel.get(), "CoreGui.RobloxGui.Modules.PlayerList.PlayerListManager");

	std::string initscript = GetInitScript();
	std::string script = "script.Parent=nil;task.spawn(function()" + initscript + "\nend);while true do task.wait(9e9) end";
	std::string bytecode = Compile(script);

	VRNavigation->UnlockModule();
	VRNavigation->SetBytecode(bytecode);
	PlayerListManager->SpoofWith(VRNavigation->Self());

	_process.Write<uintptr_t>(_process.GetAddress() + Offsets::FFlags::WebSocketServiceEnableClientCreation, 1); // enable websocket client creation, important for communication

	_process.FocusWindow();

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
		throw std::runtime_error("Inject(): failed to send inputs");

	std::this_thread::sleep_for(std::chrono::microseconds(250000));


	PlayerListManager->SpoofWith(PlayerListManager->Self());
}

void Client::Execute(const std::string& source) const {
	json request;

	request["action"] = "execute";
	request["source"] = source;

	json response = _server->SendAndReceive(request, _process.GetProcessId());

	if (!response["success"])
		throw std::runtime_error(response["message"]);
}