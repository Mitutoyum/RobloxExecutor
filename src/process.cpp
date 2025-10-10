#include <Windows.h>
#include <TlHelp32.h>
#include <vector>

#include "executor/process.h"

std::vector<DWORD> GetProcessIds(const wchar_t* name) {
	std::vector<DWORD> PIDs;

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (snapshot == INVALID_HANDLE_VALUE) {
		return PIDs;
	}

	PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };

	if (Process32FirstW(snapshot, &entry)) {
		if (_wcsicmp(name, entry.szExeFile) == 0) {
			PIDs.push_back(entry.th32ProcessID);
		}
		while (Process32NextW(snapshot, &entry)) {
			if (_wcsicmp(name, entry.szExeFile) == 0) {
				PIDs.push_back(entry.th32ProcessID);
			}
		}
	}

	CloseHandle(snapshot);
	return PIDs;

}

uintptr_t GetBaseAddress(DWORD PID) {
	uintptr_t base_address = 0;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, PID);

	if (snapshot != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 module_entry = { sizeof(module_entry) };
		if (Module32First(snapshot, &module_entry)) {
			base_address = reinterpret_cast<uintptr_t>(module_entry.modBaseAddr);
		}
	}

	CloseHandle(snapshot);
	return base_address;
}

HWND GetWindowFromProcessId(DWORD PID) {
	struct HandleData {
		DWORD processId;
		HWND windowHandle;
	};

	HandleData data = { PID, nullptr };

	auto EnumWindowsCallback = [](HWND hwnd, LPARAM lParam) -> BOOL {
		HandleData& data = *reinterpret_cast<HandleData*>(lParam);
		DWORD windowPid = 0;
		GetWindowThreadProcessId(hwnd, &windowPid);

		if (windowPid == data.processId) {
			if (IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
				data.windowHandle = hwnd;
				return FALSE;
			}
		}
		return TRUE;
	};

	EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
	return data.windowHandle;
}

void EnableVirtualTerminal() {
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (handle == INVALID_HANDLE_VALUE) return;

	DWORD mode = 0;
	if (!GetConsoleMode(handle, &mode)) return;

	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(handle, mode);
}