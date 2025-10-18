

#include <Windows.h>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <ranges>
#include <iostream>

#include "executor/instance.h"
#include "executor/process.h"
#include "executor/offsets.h"
#include "executor/bytecode.h"

Instance::Instance(uintptr_t address, const Process* process) : _address(address), _process(process) {}

std::unique_ptr<Instance> Instance::New(uintptr_t address, const Process* process) {
	return std::make_unique<Instance>(address, process);
}

uintptr_t Instance::Self() const {
	return _address;
}

std::string Instance::Name() const {
	uintptr_t name_ptr = ReadFrom<uintptr_t>(Offsets::Instance::Name);
	return _process->Read<std::string>(name_ptr);
}

std::string Instance::ClassName() const {
	uintptr_t class_descriptor = ReadFrom<uintptr_t>(Offsets::Instance::ClassDescriptor);
	uintptr_t classname_ptr = _process->Read<uintptr_t>(class_descriptor + Offsets::Instance::ClassName);

	if (!classname_ptr) {
		return "";
	}

	return _process->Read<std::string>(classname_ptr);
}
	
std::unique_ptr<Instance> Instance::Parent() const {
	uintptr_t parent = ReadFrom<uintptr_t>(Offsets::Instance::Parent);
	return std::make_unique<Instance>(parent, _process);
}

std::vector<uintptr_t> Instance::GetChildrenAddresses() const {
	std::vector<uintptr_t> children_addresses;

	uintptr_t children_start = ReadFrom<uintptr_t>(Offsets::Instance::ChildrenStart);
	uintptr_t children_end = _process->Read<uintptr_t>(children_start + Offsets::Instance::ChildrenEnd);

	for (auto child_address = _process->Read<uintptr_t>(children_start); child_address < children_end; child_address += 0x10) {
		children_addresses.push_back(_process->Read<uintptr_t>(child_address));
	}

	return children_addresses;
}

std::vector<std::unique_ptr<Instance>> Instance::GetChildren() const {
	std::vector<uintptr_t> children_addresses = GetChildrenAddresses();
	std::vector<std::unique_ptr<Instance>> children;
	for (const auto& child_address : children_addresses) {
		children.push_back(std::make_unique<Instance>(child_address, _process));
	}
	return children;
}

uintptr_t Instance::FindFirstChildAddress(const std::string_view name) const {
	for (const auto& child_address : GetChildrenAddresses()) {
		if (_process->Read<std::string>(_process->Read<uintptr_t>(child_address + Offsets::Instance::Name)) == name)
		{
			return child_address;
		}
	}
	return 0;
}

std::unique_ptr<Instance> Instance::FindFirstChild(const std::string_view name, bool allow_exceptions) const {
	uintptr_t child_address = FindFirstChildAddress(name);
	if (child_address)
		return std::make_unique<Instance>(child_address, _process);
	if (allow_exceptions)
		throw std::runtime_error("FindFirstChild(): failed to find " + std::string(name) + " under " + Name());
	return nullptr;
}

uintptr_t Instance::FindFirstChildOfClassAddress(const std::string_view classname) const {
	for (const auto& child_address : GetChildrenAddresses()) {
		uintptr_t class_descriptor = _process->Read<uintptr_t>(child_address + Offsets::Instance::ClassDescriptor);
		uintptr_t classname_ptr = _process->Read<uintptr_t>(class_descriptor + Offsets::Instance::ClassName);

		if (!classname_ptr) continue;

		if (_process->Read<std::string>(classname_ptr) == classname) {
			return child_address;
		}
	}

	return 0;
}

std::unique_ptr<Instance> Instance::FindFirstChildOfClass(const std::string_view classname, bool allow_exceptions) const {
	uintptr_t child_address = FindFirstChildOfClassAddress(classname);

	if (child_address)
		return std::make_unique<Instance>(child_address, _process);
	if (allow_exceptions)
		throw std::runtime_error("FindFirstChildOfClass(): failed to find any " + std::string(classname) + " class " + " under " + Name());
	return nullptr;

}


std::unique_ptr<Instance> Instance::FindFirstChildFromPath(const Instance* datamodel, const std::string& path) {
	std::unique_ptr<Instance> last;

	for (const auto& child : std::views::split(path, '.')) {
		std::string token(child.begin(), child.end());

		if (token.empty()) continue;

		if (!last) {
			last = datamodel->FindFirstChild(token);
			continue;
		}
		last = last->FindFirstChild(token);
	}

	return std::move(last);
}




std::string Instance::GetBytecode() const {
	if (ClassName() != "LocalScript" && ClassName() != "ModuleScript")
		throw std::runtime_error("GetBytecode(): " + Name() + " is not a LocalScript or a ModuleScript");


	uintptr_t embedded_offset = (ClassName() == "LocalScript") ? Offsets::LocalScript::ByteCode : Offsets::ModuleScript::ByteCode;
	uintptr_t embedded_ptr = ReadFrom<uintptr_t>(embedded_offset);

	uintptr_t bytecode_ptr = _process->Read<uintptr_t>(embedded_ptr + Offsets::ByteCode::Pointer);
	uint64_t bytecode_size = _process->Read<uint64_t>(embedded_ptr + Offsets::ByteCode::Size);

	std::string bytecode;
	bytecode.resize(bytecode_size);

	bytecode = _process->Read<std::string>(bytecode_ptr, bytecode_size, false);

	return Bytecode::decompress(bytecode);
}


void Instance::SetBytecode(const std::string& bytecode) const {
	if (ClassName() != "LocalScript" && ClassName() != "ModuleScript")
		throw std::runtime_error("SetBytecode(): " + Name() + " is not a LocalScript or a ModuleScript");

	uintptr_t embedded_offset = (ClassName() == "LocalScript") ? Offsets::LocalScript::ByteCode : Offsets::ModuleScript::ByteCode;
	uintptr_t embedded_ptr = ReadFrom<uintptr_t>(embedded_offset);


	if (!_process->Write<std::string>(embedded_ptr + Offsets::ByteCode::Pointer, bytecode, embedded_ptr + Offsets::ByteCode::Size))
		throw std::runtime_error("SetBytecode(): failed to set bytecode " + Name());
}

void Instance::UnlockModule() const {
	if (ClassName() != "ModuleScript") {
		throw std::runtime_error("SetBytecode(): " + Name() + " is not a ModuleScript");
	}

	if (!(WriteTo<uintptr_t>(Offsets::ModuleScript::ModuleFlags, 0x100000000) &&
		WriteTo<uintptr_t>(Offsets::ModuleScript::IsCoreScript, 0x1))) {
		throw std::runtime_error("Failed to unlock module " + Name());
	}
}

void Instance::SpoofWith(uintptr_t instance_ptr) const {

	if (!WriteTo<uintptr_t>(0x8, instance_ptr)) {
		throw std::runtime_error("SpoofWith(): failed to spoof instance " + Name() + " with " + _process->Read<std::string>(ReadFrom<uintptr_t>(Offsets::Instance::Name)));
	}
}
