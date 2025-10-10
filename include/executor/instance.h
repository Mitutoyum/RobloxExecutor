#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>


class Instance {
private:
	const uintptr_t _address;
	HANDLE _handle;
public:
	Instance(uintptr_t address, HANDLE handle);

	uintptr_t Self() const;
	std::string Name() const;
	std::string ClassName() const;
	std::unique_ptr<Instance> Parent() const;

	std::vector<uintptr_t> GetChildrenAddresses() const;
	uintptr_t FindFirstChildAddress(const std::string_view name) const;
	uintptr_t FindFirstChildOfClassAddress(const std::string_view classname) const;


	std::vector<std::unique_ptr<Instance>> GetChildren() const;
	std::unique_ptr<Instance> FindFirstChild(const std::string_view name, bool allow_exceptions = true) const;
	std::unique_ptr<Instance> FindFirstChildOfClass(const std::string_view classname, bool allow_exceptions = true) const;

	std::string GetBytecode() const;
	void SetBytecode(const std::string& bytecode) const;

	void UnlockModule() const;
	void SpoofWith(uintptr_t instance_ptr) const;
};