

#include <Windows.h>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

#include "executor/instance.h"
#include "executor/process.h"
#include "executor/offsets.h"
#include "executor/bytecode.h"

Instance::Instance(uintptr_t address, HANDLE handle) : _address(address), _handle(handle) {}

uintptr_t Instance::Self() const {
	return _address;
}

std::string Instance::Name() const {
	uintptr_t name_ptr = Read<uintptr_t>(_address + Offsets::Instance::Name, _handle);
	return Read<std::string>(name_ptr, _handle);
}

std::string Instance::ClassName() const {
	uintptr_t class_descriptor = Read<uintptr_t>(_address + Offsets::Instance::ClassDescriptor, _handle);
	uintptr_t classname_ptr = Read<uintptr_t>(class_descriptor + Offsets::Instance::ClassName, _handle);

	if (!classname_ptr) {
		return "";
	}

	return Read<std::string>(classname_ptr, _handle);
}
	
std::unique_ptr<Instance> Instance::Parent() const {
	uintptr_t parent = Read<uintptr_t>(_address + Offsets::Instance::Parent, _handle);
	return std::make_unique<Instance>(parent, _handle);
}

std::vector<uintptr_t> Instance::GetChildrenAddresses() const {
	std::vector<uintptr_t> children_addresses;

	uintptr_t children_start = Read<uintptr_t>(_address + Offsets::Instance::ChildrenStart, _handle);
	uintptr_t children_end = Read<uintptr_t>(children_start + Offsets::Instance::ChildrenEnd, _handle);

	for (auto child_address = Read<uintptr_t>(children_start, _handle); child_address < children_end; child_address += 0x10) {
		children_addresses.push_back(Read<uintptr_t>(child_address, _handle));
	}

	return children_addresses;
}

std::vector<std::unique_ptr<Instance>> Instance::GetChildren() const {
	std::vector<uintptr_t> children_addresses = GetChildrenAddresses();
	std::vector<std::unique_ptr<Instance>> children;
	for (const auto& child_address : children_addresses) {
		children.push_back(std::make_unique<Instance>(child_address, _handle));
	}
	return children;
}

uintptr_t Instance::FindFirstChildAddress(const std::string_view name) const {
	for (const auto& child_address : GetChildrenAddresses()) {
		if (Read<std::string>(Read<uintptr_t>(child_address + Offsets::Instance::Name, _handle), _handle) == name)
		{
			return child_address;
		}
	}

	return 0;

}

std::unique_ptr<Instance> Instance::FindFirstChild(const std::string_view name, bool allow_exceptions) const {
	uintptr_t child_address = FindFirstChildAddress(name);
	if (child_address)
		return std::make_unique<Instance>(child_address, _handle);
	if (allow_exceptions)
		throw std::runtime_error("Failed to find " + std::string(name) + " under " + Name());
	return nullptr;
}

uintptr_t Instance::FindFirstChildOfClassAddress(const std::string_view classname) const {
	for (const auto& child_address : GetChildrenAddresses()) {
		uintptr_t class_descriptor = Read<uintptr_t>(child_address + Offsets::Instance::ClassDescriptor, _handle);
		uintptr_t classname_ptr = Read<uintptr_t>(class_descriptor + Offsets::Instance::ClassName, _handle);

		if (!classname_ptr) continue;

		if (Read<std::string>(classname_ptr, _handle) == classname) {
			return child_address;
		}
	}

	return 0;

}

std::unique_ptr<Instance> Instance::FindFirstChildOfClass(const std::string_view classname, bool allow_exceptions) const {
	uintptr_t child_address = FindFirstChildOfClassAddress(classname);

	if (child_address)
		return std::make_unique<Instance>(child_address, _handle);
	if (allow_exceptions)
		throw std::runtime_error("Failed to find any " + std::string(classname) + " class " + " under " + Name());
	return nullptr;

}



std::string Instance::GetBytecode() const {
	if (ClassName() != "LocalScript" && ClassName() != "ModuleScript")
		return "";

	uintptr_t embedded_offset = (ClassName() == "LocalScript") ? Offsets::LocalScript::ByteCode : Offsets::ModuleScript::ByteCode;
	uintptr_t embedded_ptr = Read<uintptr_t>(_address + embedded_offset, _handle);

	uintptr_t bytecode_ptr = Read<uintptr_t>(embedded_ptr + Offsets::ByteCode::Pointer, _handle);
	uint64_t bytecode_size = Read<uint64_t>(embedded_ptr + Offsets::ByteCode::Size, _handle);

	std::string bytecode;
	bytecode.resize(bytecode_size);

	bytecode = Read<std::string>(bytecode_ptr, _handle, bytecode_size, false);

	return Bytecode::decompress(bytecode);
}


void Instance::SetBytecode(const std::string& bytecode) const {
	if (ClassName() != "LocalScript" && ClassName() != "ModuleScript")
		throw std::runtime_error(Name() + " is not a localscript or a modulescript");

	std::uintptr_t embedded_offset = (ClassName() == "LocalScript") ? Offsets::LocalScript::ByteCode : Offsets::ModuleScript::ByteCode;
	std::uintptr_t embedded_ptr = Read<uintptr_t>(_address + embedded_offset, _handle);

	if (!Write<std::string>(embedded_ptr + Offsets::ByteCode::Pointer, bytecode, _handle, embedded_ptr + Offsets::ByteCode::Size))
		throw std::runtime_error("Failed to set bytecode " + Name());
}

void Instance::UnlockModule() const {
	if (ClassName() != "ModuleScript") {
		throw std::runtime_error(Name() + " is not a modulescript");
	}

	if (!(Write<uintptr_t>(_address + Offsets::ModuleScript::ModuleFlags, 0x100000000, _handle) &&
		Write<uintptr_t>(_address + Offsets::ModuleScript::IsCoreScript, 0x1, _handle))) {
		throw std::runtime_error("Failed to unlock module " + Name());
	}
}

void Instance::SpoofWith(uintptr_t instance_ptr) const {

	if (!Write<uintptr_t>(_address + 0x8, instance_ptr, _handle)) {
		throw std::runtime_error("Failed to spoof instance " + Name() + " with " + Read<std::string>(Read<uintptr_t>(_address + Offsets::Instance::Name, _handle), _handle));
	}
}
