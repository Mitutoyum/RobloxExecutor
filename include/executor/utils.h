#pragma once

#include <Windows.h>
#include <string>

#include "nlohmann/json.hpp"
using nlohmann::json;

#include "executor/process.h"

uintptr_t GetDatamodel(const Process* process);
HMODULE GetModule();

std::string Compile(const std::string& source);

void ReplaceString(std::string& data, const std::string_view replace, const std::string_view replacement);
void REPLPrint(const std::string& message);

std::string GenerateGUID();

void CheckRequiredKeys(const json& data, const std::vector<std::string>& keys);