#pragma once

#include <Windows.h>
#include <string>

#define CHECK(condition, error_message, result) \
	if (!(condition)) { \
		result.success = false; \
		result.message = error_message; \
		return result; \
	}

struct Result {
    bool success;
    std::string message;
};

uintptr_t GetDatamodel(uintptr_t address, HANDLE handle);
HMODULE GetModule();

Result Compile(const std::string& source);
bool IsCompilable(const std::string& source);

void ReplaceString(std::string& data, const std::string_view replace, const std::string_view replacement);

void REPLPrint(const std::string& message);

std::string GenerateGUID();
