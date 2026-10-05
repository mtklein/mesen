#pragma once
#include "pch.h"
#include "Debugger/DebugTypes.h"
#include "Debugger/ScriptHost.h"
#include "Utilities/SimpleLock.h"
#include "Shared/EventType.h"

class Debugger;
enum class MemoryOperationType;

class ScriptManager
{
private:
	Debugger* _debugger = nullptr;
	bool _hasScript = false;
	SimpleLock _scriptLock;
	int _nextScriptId = 0;
	uint32_t _cpuMemoryCallbackMask[3] = {}; //per CallbackType (read, write, exec): one bit per CpuType that has one
	bool _isPpuMemoryCallbackEnabled = false;
	vector<unique_ptr<ScriptHost>> _scripts;

public:
	void RefreshMemoryCallbackFlags();
	void RefreshScriptPages();

	ScriptManager(Debugger* debugger);
	~ScriptManager();

	__forceinline bool HasScript() { return _hasScript; }
	int32_t LoadScript(string name, string path, string content, int32_t scriptId);
	void RemoveScript(int32_t scriptId);
	string GetScriptLog(int32_t scriptId);
	void ProcessEvent(EventType type, CpuType cpuType);

	void EnableCpuMemoryCallbacks(CpuType cpuType, CallbackType type);
	bool HasCpuMemoryCallbacks(CpuType cpuType)
	{
		return _scripts.size() && ((_cpuMemoryCallbackMask[0] | _cpuMemoryCallbackMask[1] | _cpuMemoryCallbackMask[2]) & (1u << (int)cpuType));
	}
	__forceinline bool HasCpuMemoryCallbacks(CpuType cpuType, CallbackType type)
	{
		return _scripts.size() && (_cpuMemoryCallbackMask[(int)type] & (1u << (int)cpuType));
	}
	//Whether ProcessMemoryOperation(processExec = false) has any callback to run for this access
	__forceinline bool HasCpuMemoryCallbacks(CpuType cpuType, MemoryOperationType opType)
	{
		switch(opType) {
			case MemoryOperationType::Read:
			case MemoryOperationType::DmaRead:
			case MemoryOperationType::PpuRenderingRead:
			case MemoryOperationType::DummyRead:
				return HasCpuMemoryCallbacks(cpuType, CallbackType::Read);

			case MemoryOperationType::Write:
			case MemoryOperationType::DummyWrite:
			case MemoryOperationType::DmaWrite:
				return HasCpuMemoryCallbacks(cpuType, CallbackType::Write);

			default:
				return false;
		}
	}

	void EnablePpuMemoryCallbacks() { _isPpuMemoryCallbackEnabled = true; }
	bool HasPpuMemoryCallbacks() { return _scripts.size() && _isPpuMemoryCallbackEnabled; }

	template<typename T>
	__forceinline void ProcessMemoryOperation(AddressInfo relAddr, T& value, MemoryOperationType type, CpuType cpuType, bool processExec)
	{
		switch(type) {
			case MemoryOperationType::Read:
			case MemoryOperationType::DmaRead:
			case MemoryOperationType::PpuRenderingRead:
			case MemoryOperationType::DummyRead:
				for(unique_ptr<ScriptHost>& script : _scripts) {
					script->CallMemoryCallback(relAddr, value, CallbackType::Read, cpuType);
				}
				break;

			case MemoryOperationType::Write:
			case MemoryOperationType::DummyWrite:
			case MemoryOperationType::DmaWrite:
				for(unique_ptr<ScriptHost>& script : _scripts) {
					script->CallMemoryCallback(relAddr, value, CallbackType::Write, cpuType);
				}
				break;

			case MemoryOperationType::ExecOpCode:
			case MemoryOperationType::ExecOperand:
				if(processExec) {
					for(unique_ptr<ScriptHost>& script : _scripts) {
						script->CallMemoryCallback(relAddr, value, CallbackType::Exec, cpuType);
					}
				}
				break;

			default: break;
		}
	}
};