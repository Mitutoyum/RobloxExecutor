
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

Result Client::GetInitScript() const {
	Result result;
	std::string initscript;

	HMODULE module = GetModule();
	CHECK(module, "Failed to get module handle for initscript", result);

	HRSRC resource = FindResource(module, MAKEINTRESOURCE(INIT), MAKEINTRESOURCE(LUA));
	CHECK(resource, "Failed to find initscript", result);

	HGLOBAL data = LoadResource(module, resource);
	CHECK(data, "Failed to load initscript", result);

	DWORD size = SizeofResource(module, resource);
	char* final_data = static_cast<char*>(LockResource(data));

	initscript.assign(final_data, size);
	ReplaceString(initscript, "%PROCESS_ID%", std::to_string(_PID));

	result.message = initscript;

	return result;
}



Result Client::Initialize() const {
	Result result;

	auto Datamodel = std::make_unique<Instance>(GetDatamodel(_address, _handle), _handle);
	CHECK(Datamodel->Self(), "Failed to get datamodel", result);

	std::unique_ptr<Instance> VRNavigation;
	std::unique_ptr<Instance> PlayerListManager;


	try {
		VRNavigation = Datamodel->FindFirstChild("StarterPlayer")
			->FindFirstChild("StarterPlayerScripts")
			->FindFirstChild("PlayerModule")
			->FindFirstChild("ControlModule")
			->FindFirstChild("VRNavigation");

		PlayerListManager = Datamodel->FindFirstChild("CoreGui")
			->FindFirstChild("RobloxGui")
			->FindFirstChild("Modules")
			->FindFirstChild("PlayerList")
			->FindFirstChild("PlayerListManager");
	}
	catch (const std::exception& exception) {
		result.success = false;
		result.message = std::string(exception.what());
		return result;
	}

	Result initscript = GetInitScript();
	CHECK(initscript.success, initscript.message, result);

	std::string script = "script.Parent=nil;task.spawn(function()" + initscript.message + "\nend);while true do task.wait(9e9) end";
	Result compile_result = Compile(script);
	CHECK(compile_result.success, compile_result.message, result);

	CHECK(VRNavigation->UnlockModule(), "Failed to unlock module", result);

	CHECK(VRNavigation->SetBytecode(compile_result.message), "Failed to set bytecode", result);

	CHECK(PlayerListManager->SpoofWith(VRNavigation->Self()), "Failed to spoof instance", result);

	Write<uintptr_t>(_address + 0x607cf20, 1, _handle); // enable websocket client creation, important for communication


	CHECK(FocusWindow(), "Failed to focus window", result);


	INPUT inputs[2] = {};

	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = VK_ESCAPE;
	inputs[0].ki.wScan = MapVirtualKey(VK_ESCAPE, 0);

	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = VK_ESCAPE;
	inputs[1].ki.wScan = MapVirtualKey(VK_ESCAPE, 0);
	inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

	UINT uSent = SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
	CHECK(uSent == ARRAYSIZE(inputs), "Failed to send inputs", result);

	Sleep(500);

	CHECK(PlayerListManager->SpoofWith(PlayerListManager->Self()), "Failed to unspoof instance", result);

	result.success = true;

	return result;
}

bool Client::FocusWindow() const {
	HWND ClientHWND = GetWindowFromProcessId(_PID);

	if (!ClientHWND) return false;

	return SetForegroundWindow(ClientHWND);

}

Result Client::Execute(const std::string& source) const {
	Result result;
	json request;

	request["action"] = "execute";
	request["source"] = source;

	json response = _server.SendAndReceive(request, _PID);
	result.success = response["success"];

	if (!result.success) result.message = response["message"];

	return result;
}

Result Client::Loadstring(const std::string& chunk, const std::string& chunk_name, const std::string& script_name) const {
	Result result;

	auto Datamodel = std::make_unique<Instance>(GetDatamodel(_address, _handle), _handle);
	CHECK(Datamodel->Self(), "Failed to get datamodel", result);

	std::unique_ptr<Instance> Scripts;

	try {
		Scripts = Datamodel->FindFirstChildOfClass("RobloxReplicatedStorage")
			->FindFirstChild("Executor")
			->FindFirstChild("Scripts");
	}
	catch (const std::exception& exception) {
		result.success = false;
		result.message = std::string(exception.what());
		return result;
	}

	auto Script = Scripts->FindFirstChild(script_name, false);
	CHECK(Script, "Failed to find script " + script_name, result);

	Result compile_result = Compile("local function " + chunk_name + "(...)do setmetatable(getgenv and getgenv()or{},{__newindex=function(t,i,v)rawset(t,i,v)getfenv()[i]=v;end})end;" + chunk + "\nend;return " + chunk_name);

	CHECK(Script->UnlockModule(), "Failed to unlock module", result);

	CHECK(Script->SetBytecode(compile_result.message), "Failed to set bytecode", result);

	result.success = true;

	return result;
}
