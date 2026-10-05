#pragma once
#include "pch.h"
#include "Utilities/SimpleLock.h"
#include "Debugger/DebugUtilities.h"
#include "Debugger/DebugTypes.h"
#include "Debugger/DebuggerFeatures.h"
#include "Shared/SettingTypes.h"
#include "Shared/MemoryOperationType.h"
#include "Debugger/IDebugger.h"
#include "Debugger/BreakpointManager.h"
#include "Debugger/ITraceLogger.h"

class IConsole;
class Emulator;
class SnesCpu;
class SnesPpu;
class Spc;
class BaseCartridge;
class SnesMemoryManager;
class InternalRegisters;
class SnesDmaController;
class EmuSettings;

class ExpressionEvaluator;
class MemoryDumper;
class MemoryAccessCounter;
class Disassembler;
class DisassemblySearch;
class BreakpointManager;
class PpuTools;
class CodeDataLogger;
class CallstackManager;
class LabelManager;
class CdlManager;
class ScriptManager;
class Breakpoint;
class BaseEventManager;
class IAssembler;
class IDebugger;
class ITraceLogger;
class TraceLogFileSaver;
class FrozenAddressManager;
class ISerializable;

struct TraceRow;
struct BaseState;

enum class EventType;
enum class MemoryOperationType;
enum class EvalResultType : int32_t;

struct CpuInfo
{
	unique_ptr<IDebugger> Debugger;
	unique_ptr<ExpressionEvaluator> Evaluator;
	BreakpointManager* Breakpoints = nullptr; //cached for the script-only check
	ITraceLogger* TraceLogger = nullptr;
	unique_ptr<StepRequest>* Step = nullptr; //cached for the inline per-PPU-cycle check
};

class Debugger
{
private:
	Emulator* _emu = nullptr;
	IConsole* _console = nullptr;

	EmuSettings* _settings = nullptr;

	CpuInfo _debuggers[(int)DebugUtilities::GetLastCpuType() + 1];

	CpuType _mainCpuType = CpuType::Snes;
	unordered_set<CpuType> _cpuTypes;
	ConsoleType _consoleType = ConsoleType::Snes;

	unique_ptr<ScriptManager> _scriptManager;
	unique_ptr<MemoryDumper> _memoryDumper;
	unique_ptr<MemoryAccessCounter> _memoryAccessCounter;
	unique_ptr<CodeDataLogger> _codeDataLogger;
	unique_ptr<Disassembler> _disassembler;
	unique_ptr<DisassemblySearch> _disassemblySearch;
	unique_ptr<LabelManager> _labelManager;
	unique_ptr<CdlManager> _cdlManager;

	unique_ptr<TraceLogFileSaver> _traceLogSaver;

	SimpleLock _logLock;
	std::list<string> _debuggerLog;

	atomic<bool> _executionStopped;
	atomic<uint32_t> _breakRequestCount;
	atomic<uint32_t> _suspendRequestCount;

	DebugControllerState _inputOverrides[8] = {};

	bool _waitForBreakResume = false;

	//Script-only mode (OT6 mesen-lean patch): set by MESEN_SCRIPT_ONLY=1 while no
	//debugger window is open. The SNES/SPC debuggers then skip their per-access
	//bookkeeping (access counters, CDL, call stack, event log, disassembly cache)
	//and keep only what Lua callbacks and break requests need.
	bool _scriptOnly = false;
	uint32_t _scriptCallbackMask[3] = {}; //ScriptManager's per-CallbackType masks (read, write, exec; one bit per CpuType), mirrored for the inline checks
	uint8_t _scriptPages[3][0x10000 / 8] = {}; //per CallbackType, the union of every script's 256-byte page filter (ScriptingContext)
	bool _scriptAnyPage[3] = {};
	void UpdateScriptOnly();
	template<CpuType type> __forceinline bool CanSkipCpuDebugger();

	void Reset();

	__noinline bool ProcessStepBack(IDebugger* debugger);

	template<CpuType type, typename DebuggerType> DebuggerType* GetDebugger();
	template<CpuType type> uint64_t GetCpuCycleCount();
	template<CpuType type, typename T> void ProcessScripts(uint32_t addr, T& value, MemoryOperationType opType);
	template<CpuType type, typename T> void ProcessScripts(uint32_t addr, T& value, MemoryType memType, MemoryOperationType opType);

	bool IsDebugWindowOpened(CpuType cpuType);
	bool IsBreakOptionEnabled(BreakSource src);
	template<CpuType type> void SleepOnBreakRequest();

	void ClearPendingBreakExceptions();

	bool IsBreakpointForbidden(BreakSource source, CpuType sourceCpu, MemoryOperationInfo* operation);

public:
	Debugger(Emulator* emu, IConsole* console);
	~Debugger();
	void Release();

	template<CpuType type> void ProcessInstruction();
	template<CpuType type, uint8_t accessWidth = 1, MemoryAccessFlags flags = MemoryAccessFlags::None, typename T> void ProcessMemoryRead(uint32_t addr, T& value, MemoryOperationType opType);
	template<CpuType type, uint8_t accessWidth = 1, MemoryAccessFlags flags = MemoryAccessFlags::None, typename T> bool ProcessMemoryWrite(uint32_t addr, T& value, MemoryOperationType opType);

	template<CpuType cpuType, MemoryType memType, MemoryOperationType opType, typename T> void ProcessMemoryAccess(uint32_t addr, T& value);

	template<CpuType type> void ProcessIdleCycle();
	template<CpuType type> void ProcessHaltedCpu();
	template<CpuType type, typename T> void ProcessPpuRead(uint16_t addr, T& value, MemoryType memoryType, MemoryOperationType opType);
	template<CpuType type, typename T> void ProcessPpuWrite(uint16_t addr, T& value, MemoryType memoryType);
	template<CpuType type> void ProcessPpuCycle();
	template<CpuType type> void ProcessInterrupt(uint32_t originalPc, uint32_t currentPc, bool forNmi);

	void InternalProcessInterrupt(CpuType cpuType, IDebugger& dbg, StepRequest& stepRequest, AddressInfo& src, uint32_t srcAddr, AddressInfo& dest, uint32_t destAddr, AddressInfo& ret, uint32_t retAddr, uint32_t retSp, bool forNmi);

	void ProcessEvent(EventType type, std::optional<CpuType> cpuType);

	void ProcessConfigChange();

	__forceinline bool IsScriptOnly() { return _scriptOnly; }

	void SetScriptCallbackMask(int callbackType, uint32_t mask) { _scriptCallbackMask[callbackType] = mask; }
	void SetScriptPages(int callbackType, const uint8_t* pages, bool anyPage)
	{
		memcpy(_scriptPages[callbackType], pages, sizeof(_scriptPages[0]));
		_scriptAnyPage[callbackType] = anyPage;
	}

	//Whether a script may have a callback of this type (0 read, 1 write, 2 exec) at this relative address on this CPU
	template<CpuType type> __forceinline bool HasScriptCallbackAt(int callbackType, uint32_t addr)
	{
		if(!(_scriptCallbackMask[callbackType] & (1u << (int)type))) {
			return false;
		}
		uint32_t page = addr >> 8;
		return _scriptAnyPage[callbackType] || (page < 0x10000 && (_scriptPages[callbackType][page >> 3] & (1 << (page & 7))));
	}

	//Script-only, nothing pending on this CPU (no step, break, breakpoint, trace log or step back): the
	//same test the out-of-line ProcessMemoryRead/Write make before skipping the per-CPU debugger
	template<CpuType type> __forceinline bool IsQuietCpu()
	{
		if constexpr(type == CpuType::Snes || type == CpuType::Sa1 || type == CpuType::Spc) {
			CpuInfo& cpu = _debuggers[(int)type];
			StepRequest* step = cpu.Step->get();
			return _scriptOnly && !step->HasRequest && step->BreakNeeded == BreakType::None &&
				!cpu.Breakpoints->HasBreakpoints() && !cpu.TraceLogger->IsEnabled() && !cpu.Debugger->IsStepBack();
		} else {
			return false;
		}
	}

	//Whether a script has a callback this access would run (what ProcessScripts with processExec = false can call)
	template<CpuType type> __forceinline bool HasScriptCallbackFor(uint32_t addr, MemoryOperationType opType)
	{
		switch(opType) {
			case MemoryOperationType::Read:
			case MemoryOperationType::DmaRead:
			case MemoryOperationType::PpuRenderingRead:
			case MemoryOperationType::DummyRead:
				return HasScriptCallbackAt<type>(0, addr);
			case MemoryOperationType::Write:
			case MemoryOperationType::DummyWrite:
			case MemoryOperationType::DmaWrite:
				return HasScriptCallbackAt<type>(1, addr);
			default:
				return false;
		}
	}

	//The memory hooks' fast path: a quiet CPU with no script callback for this access has nothing to do
	template<CpuType type, uint8_t accessWidth = 1, MemoryAccessFlags flags = MemoryAccessFlags::None, typename T>
	__forceinline void ProcessMemoryReadInline(uint32_t addr, T& value, MemoryOperationType opType)
	{
		if(IsQuietCpu<type>() && !HasScriptCallbackFor<type>(addr, opType)) {
			return;
		}
		ProcessMemoryRead<type, accessWidth, flags>(addr, value, opType);
	}

	template<CpuType type, uint8_t accessWidth = 1, MemoryAccessFlags flags = MemoryAccessFlags::None, typename T>
	__forceinline bool ProcessMemoryWriteInline(uint32_t addr, T& value, MemoryOperationType opType)
	{
		if(IsQuietCpu<type>() && !HasScriptCallbackFor<type>(addr, opType)) {
			return !_debuggers[(int)type].Debugger->GetFrozenAddressManager().IsFrozenAddress(addr);
		}
		return ProcessMemoryWrite<type, accessWidth, flags>(addr, value, opType);
	}

	//Per instruction, with the instruction's address: a quiet CPU with no break to service and no exec
	//callback on this page has nothing to do (the test ProcessInstruction makes, without the call)
	template<CpuType type> __forceinline void ProcessInstructionInline(uint32_t pc)
	{
		if(IsQuietCpu<type>() && !HasPendingBreak() && !HasScriptCallbackAt<type>(2, pc)) {
			return;
		}
		ProcessInstruction<type>();
	}

	template<CpuType type> __forceinline void ProcessIdleCycleInline()
	{
		if(IsQuietCpu<type>()) {
			_debuggers[(int)type].Debugger->InstructionProgress.LastMemOperation.Type = MemoryOperationType::Idle;
			return;
		}
		ProcessIdleCycle<type>();
	}

	//Script-only, no PPU step pending: ProcessPpuCycle has nothing to do (checked inline, every PPU cycle)
	template<CpuType type> __forceinline bool SkipsPpuCycle()
	{
		return _scriptOnly && !(*_debuggers[(int)type].Step)->HasRequest;
	}
	__forceinline bool HasPendingBreak() { return _breakRequestCount || _waitForBreakResume; }

	void GetTokenList(CpuType cpuType, char* tokenList);
	int64_t EvaluateExpression(string expression, CpuType cpuType, EvalResultType& resultType, bool useCache);

	void Run();
	void PauseOnNextFrame();
	void Step(CpuType cpuType, int32_t stepCount, StepType type, BreakSource source = BreakSource::Unspecified);
	bool IsPaused();
	bool IsExecutionStopped();

	bool HasBreakRequest();
	void BreakRequest(bool release);
	void ResetSuspendCounter();
	void SuspendDebugger(bool release);

	__noinline void BreakImmediately(CpuType sourceCpu, BreakSource source);

	template<uint8_t accessWidth = 1> void ProcessPredictiveBreakpoint(CpuType sourceCpu, BreakpointManager* bpManager, MemoryOperationInfo& operation, AddressInfo& addressInfo);
	template<uint8_t accessWidth = 1> void ProcessBreakConditions(CpuType sourceCpu, StepRequest& step, BreakpointManager* bpManager, MemoryOperationInfo& operation, AddressInfo& addressInfo);

	void SleepUntilResume(CpuType sourceCpu, BreakSource source, MemoryOperationInfo* operation = nullptr, int breakpointId = -1);

	void GetCpuState(BaseState& dstState, CpuType cpuType);
	void SetCpuState(BaseState& srcState, CpuType cpuType);
	ISerializable* GetSerializableCpu(CpuType cpuType);
	BaseState& GetCpuStateRef(CpuType cpuType);

	void GetPpuState(BaseState& state, CpuType cpuType);
	void SetPpuState(BaseState& srcState, CpuType cpuType);

	void GetConsoleState(BaseState& state, ConsoleType consoleType);

	DebuggerFeatures GetDebuggerFeatures(CpuType cpuType);
	uint32_t GetProgramCounter(CpuType cpuType, bool forInstStart);
	uint8_t GetCpuFlags(CpuType cpuType, uint32_t addr);
	CpuInstructionProgress GetInstructionProgress(CpuType cpuType);
	void SetProgramCounter(CpuType cpuType, uint32_t addr);

	AddressInfo GetAbsoluteAddress(AddressInfo relAddress);
	AddressInfo GetRelativeAddress(AddressInfo absAddress, CpuType cpuType);

	bool HasCpuType(CpuType cpuType);

	void SetBreakpoints(Breakpoint breakpoints[], uint32_t length);

	void SetInputOverrides(uint32_t index, DebugControllerState state);
	void GetAvailableInputOverrides(uint8_t* availableIndexes);

	void Log(string message);
	string GetLog();

	bool SaveRomToDisk(string filename, bool saveAsIps, CdlStripOption stripOption);

	void ClearExecutionTrace();
	uint32_t GetExecutionTrace(TraceRow output[], uint32_t startOffset, uint32_t maxLineCount);

	CpuType GetMainCpuType() { return _mainCpuType; }
	IDebugger* GetCpuDebugger(CpuType cpuType);
	IDebugger* GetMainDebugger();

	TraceLogFileSaver* GetTraceLogFileSaver() { return _traceLogSaver.get(); }
	MemoryDumper* GetMemoryDumper() { return _memoryDumper.get(); }
	MemoryAccessCounter* GetMemoryAccessCounter() { return _memoryAccessCounter.get(); }
	Disassembler* GetDisassembler() { return _disassembler.get(); }
	DisassemblySearch* GetDisassemblySearch() { return _disassemblySearch.get(); }
	LabelManager* GetLabelManager() { return _labelManager.get(); }
	CdlManager* GetCdlManager() { return _cdlManager.get(); }
	ScriptManager* GetScriptManager() { return _scriptManager.get(); }
	IConsole* GetConsole() { return _console; }
	Emulator* GetEmulator() { return _emu; }

	FrozenAddressManager* GetFrozenAddressManager(CpuType cpuType);
	ITraceLogger* GetTraceLogger(CpuType cpuType);
	PpuTools* GetPpuTools(CpuType cpuType);
	BaseEventManager* GetEventManager(CpuType cpuType);
	CallstackManager* GetCallstackManager(CpuType cpuType);
	IAssembler* GetAssembler(CpuType cpuType);
};
