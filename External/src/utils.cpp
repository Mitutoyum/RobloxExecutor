#include <Windows.h>
#include <string>

#include "executor/utils.h"
#include "executor/process.h"
#include "executor/offsets.h"
#include "executor/bytecode.h"

#include "Luau/Compiler.h"
#include "Luau/BytecodeBuilder.h"
#include "Luau/BytecodeUtils.h"

#include <iostream>

uintptr_t GetDatamodel(uintptr_t address, HANDLE handle) {
    uintptr_t FakeDatamodel = Read<uintptr_t>(address + Offsets::FakeDataModel::Pointer, handle);

    if (!FakeDatamodel) return 0;

    return Read<uintptr_t>(FakeDatamodel + Offsets::FakeDataModel::RealDataModel, handle);;
}

HMODULE GetModule() {
    HMODULE hModule;
    GetModuleHandleEx(
        (0x00000004),
        (LPCTSTR)GetModule,
        &hModule);
    return hModule;
}


Result Compile(const std::string& source)
{
    Result result;
    BytecodeEncoder encoder{};
    const std::string bytecode = Luau::compile(source, {}, {}, &encoder);

    if (bytecode[0] == '\0') {
        std::string error_message = bytecode;
        error_message.erase(std::remove(error_message.begin(), error_message.end(), '\0'), error_message.end());

        result.success = false;
        result.message = error_message;
        return result;
    }

    result.success = true;
    result.message = Bytecode::sign_bytecode(bytecode);

    return result;
}

void ReplaceString(std::string& data, const std::string_view replace, const std::string_view replacement) {
    size_t pos = data.find(replace);

    if (pos != std::string::npos) {
        data.replace(pos, replace.length(), replacement);
    }
}

void REPLPrint(const std::string& message) {
    std::cout << "\033[2K\r";
    std::cout << message << std::endl;
    std::cout << "> ";
}

std::string GenerateGUID() {
    GUID guid;
    HRESULT hresult = CoCreateGuid(&guid);

    if (SUCCEEDED(hresult)) {
        char buf[64] = { 0 };
        sprintf_s(buf, sizeof(buf),
            "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return std::string(buf);
    }
    return "";

}


Result CheckRequiredKeys(const json& data, const std::vector<std::string>& required_keys) {
    Result result;
    std::vector<std::string> missing_keys;

    for (const auto& key : required_keys) {
        if (!data.contains(key)) {
            missing_keys.push_back(key);
        }
    }

    if (!missing_keys.empty()) {
        std::string message = "Missing required keys: ";
        for (size_t i = 0; i < missing_keys.size(); ++i) {
            message += missing_keys[i];
            if (i < missing_keys.size() - 1)
                message += ", ";
        }

        result.success = false;
        result.message = message;
        return result;
    }

    result.success = true;
    return result;
}