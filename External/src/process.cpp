#include <Windows.h>
#include <TlHelp32.h>
#include <vector>

#include "executor/process.h"

std::vector<DWORD> GetProcessIds(const wchar_t* name) {
	std::vector<DWORD> PIDs;

	HANDLE SnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (SnapShot == INVALID_HANDLE_VALUE) {
		return PIDs;
	}

	PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };

	if (Process32FirstW(SnapShot, &entry)) {
		if (_wcsicmp(name, entry.szExeFile) == 0) {
			PIDs.push_back(entry.th32ProcessID);
		}
		while (Process32NextW(SnapShot, &entry)) {
			if (_wcsicmp(name, entry.szExeFile) == 0) {
				PIDs.push_back(entry.th32ProcessID);
			}
		}
	}

	CloseHandle(SnapShot);
	return PIDs;

}

uintptr_t GetBaseAddress(DWORD PID) {
	uintptr_t baseAddress = NULL;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, PID);

	if (snapshot != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 moduleEntry = { sizeof(moduleEntry) };
		//moduleEntry.dwSize = sizeof(moduleEntry);
		if (Module32First(snapshot, &moduleEntry)) {
			baseAddress = reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr);
		}
	}

	CloseHandle(snapshot);
	return baseAddress;
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
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE) return;

	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode)) return;

	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hOut, dwMode);
}