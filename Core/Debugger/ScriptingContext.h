#pragma once
#include "pch.h"
#include <deque>
#include "Utilities/SimpleLock.h"
#include "Utilities/Timer.h"
#include "Debugger/DebugTypes.h"
#include "Shared/EventType.h"

class Debugger;
struct lua_State;

enum class CallbackType
{
	Read = 0,
	Write = 1,
	Exec = 2
};

struct MemoryCallback
{
	uint32_t StartAddress;
	uint32_t EndAddress;
	CpuType Cpu;
	MemoryType MemType;
	int Reference;
};

enum class ScriptDrawSurface
{
	ConsoleScreen,
	ScriptHud
};

class ScriptingContext
{
private:
	static ScriptingContext* _context;
	lua_State* _lua = nullptr;
	Timer _timer;
	EmuSettings* _settings = nullptr;

	deque<string> _logRows;
	SimpleLock _logLock;
	bool _allowSaveState = false;

	Debugger* _debugger = nullptr;
	CpuType _defaultCpuType = {};
	MemoryType _defaultMemType = {};

	ScriptDrawSurface _drawSurface = ScriptDrawSurface::ConsoleScreen;

	static void ExecutionCountHook(lua_State* lua);
	void LuaOpenLibs(lua_State* L, bool allowIoOsAccess);
	void ProcessLuaError();
	string GetErrorMessage();
	string SerializeTable();

protected:
	string _scriptName;
	bool _initDone = false;

	vector<MemoryCallback> _callbacks[3];
	vector<int> _eventCallbacks[(int)EventType::LastValue + 1];

	//Per callback type, one bit per 256-byte page of a 24-bit relative address: set when some
	//callback's range touches the page. Lets the vast majority of accesses skip the callback
	//loop (and its address translation) without changing which callbacks run.
	//_matchAnyPage is set when a callback can't be filtered by relative address (absolute
	//memory type, or a range above 24 bits).
	uint8_t _pageFilter[3][0x10000 / 8] = {};
	bool _matchAnyPage[3] = {};
	void RefreshPageFilter(CallbackType type);

	template<typename T> void InternalCallMemoryCallback(AddressInfo relAddr, T& value, CallbackType type, CpuType cpuType);

	bool IsAddressMatch(MemoryCallback& callback, AddressInfo addr);

public:
	ScriptingContext(Debugger* debugger);
	~ScriptingContext();
	bool LoadScript(string scriptName, string path, string scriptContent, Debugger* debugger);

	void Log(string message);
	string GetLog();

	Debugger* GetDebugger();
	string GetScriptName();

	void SetDrawSurface(ScriptDrawSurface surface) { _drawSurface = surface; }
	ScriptDrawSurface GetDrawSurface() { return _drawSurface; }

	template<typename T> void CallMemoryCallback(AddressInfo relAddr, T& value, CallbackType type, CpuType cpuType);
	__forceinline bool MayHaveMemoryCallback(AddressInfo relAddr, CallbackType type)
	{
		uint32_t page = (uint32_t)relAddr.Address >> 8;
		return _matchAnyPage[(int)type] || (page < 0x10000 && (_pageFilter[(int)type][page >> 3] & (1 << (page & 7))));
	}
	const uint8_t* GetPageFilter(CallbackType type) { return _pageFilter[(int)type]; }
	bool MatchesAnyPage(CallbackType type) { return _matchAnyPage[(int)type]; }
	int CallEventCallback(EventType type, CpuType cpuType);
	bool CheckInitDone();
	bool IsSaveStateAllowed();

	CpuType GetDefaultCpuType() { return _defaultCpuType; }
	MemoryType GetDefaultMemType() { return _defaultMemType; }

	void RefreshMemoryCallbackFlags();

	void RegisterMemoryCallback(CallbackType type, int startAddr, int endAddr, MemoryType memType, CpuType cpuType, int reference);
	void UnregisterMemoryCallback(CallbackType type, int startAddr, int endAddr, MemoryType memType, CpuType cpuType, int reference);
	void RegisterEventCallback(EventType type, int reference);
	void UnregisterEventCallback(EventType type, int reference);
};
