#pragma once

#include <Windows.h>
#include <string>
#include <vector>

std::vector<DWORD> GetProcessIds(const wchar_t* name);
uintptr_t GetBaseAddress(DWORD PID);
HWND GetWindowFromProcessId(DWORD PID);
void EnableVirtualTerminal();

template <typename T>
T Read(uintptr_t address, HANDLE handle, uint64_t size = NULL, bool convert = true) {
	T buffer;
    
	ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(address), &buffer, sizeof(buffer), nullptr);

	return buffer;
}

template <>
inline std::string Read(uintptr_t address, HANDLE handle, uint64_t size, bool convert) {

    uint64_t length = size ? size : Read<uint64_t>(address + 0x10, handle);

    if (convert)
    address = (length >= 16) ? Read<uintptr_t>(address, handle) : address;


    std::string buffer;
    buffer.resize(length);

    ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(address), buffer.data(), length, nullptr);

    return buffer;
}

template <typename T>
bool Write(uintptr_t address, T buffer, HANDLE handle, uintptr_t size_ptr = NULL) {
    return WriteProcessMemory(handle, reinterpret_cast<LPVOID>(address), &buffer, sizeof(buffer), nullptr);
}

template <>
inline bool Write<std::string>(uintptr_t address, std::string buffer, HANDLE handle, uintptr_t size_ptr) {
    size_ptr = size_ptr ? size_ptr : address + 0x10;

    address = (Read<uint64_t>(size_ptr, handle) >= 16) ? Read<uintptr_t>(address, handle) : address;

    size_t bufferSize = buffer.size();

    return WriteProcessMemory(handle, reinterpret_cast<LPVOID>(address), buffer.c_str(), bufferSize, nullptr) &&
           WriteProcessMemory(handle, reinterpret_cast<LPVOID>(size_ptr), &bufferSize, sizeof(bufferSize), nullptr);
}