#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <memory>

#include "executor/process.h"


class Instance {
private:
	const Process* _process;
	uintptr_t _address;
public:
	Instance(uintptr_t address, const Process* process);

	static std::unique_ptr<Instance> New(uintptr_t address, const Process* process);

	template <typename T, typename... Params>
	T ReadFrom(uintptr_t offset, Params... params) const {
		return _process->Read<T>(_address + offset, std::forward<Params>(params)...);
	}

	template <typename T, typename... Params>
	bool WriteTo(uintptr_t offset, T buffer, Params... params) const {
		return _process->Write<T>(_address + offset, buffer, std::forward<Params>(params)...);
	}

	uintptr_t Self() const;
	std::string Name() const;
	std::string ClassName() const;
	std::unique_ptr<Instance> Parent() const;

	std::vector<uintptr_t> GetChildrenAddresses() const;
	uintptr_t FindFirstChildAddress(const std::string_view name) const;
	uintptr_t FindFirstChildOfClassAddress(const std::string_view classname) const;


	static std::unique_ptr<Instance> FindFirstChildFromPath(const Instance* datamodel, const std::string& path);


	std::vector<std::unique_ptr<Instance>> GetChildren() const;
	std::unique_ptr<Instance> FindFirstChild(const std::string_view name, bool allow_exceptions = true) const;
	std::unique_ptr<Instance> FindFirstChildOfClass(const std::string_view classname, bool allow_exceptions = true) const;

	std::string GetBytecode() const;
	void SetBytecode(const std::string& bytecode) const;

	void UnlockModule() const;
	void SpoofWith(uintptr_t instance_ptr) const;
};