#pragma once

#include <Windows.h>
#include <string>

#include "nlohmann/json.hpp"
using nlohmann::json;

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

std::string Compile(const std::string& source);

void ReplaceString(std::string& data, const std::string_view replace, const std::string_view replacement);

void REPLPrint(const std::string& message);

std::string GenerateGUID();

void CheckRequiredKeys(const json& data, const std::vector<std::string>& keys);

