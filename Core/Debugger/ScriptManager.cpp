#include "pch.h"
#include "Debugger/ScriptManager.h"
#include "Debugger/ScriptHost.h"
#include "Debugger/DebugBreakHelper.h"
#include "Debugger/Debugger.h"
#include "Shared/Emulator.h"
#include "Shared/Video/DebugHud.h"
#include "Shared/MemoryOperationType.h"

ScriptManager::ScriptManager(Debugger* debugger)
{
	_debugger = debugger;
	_hasScript = false;
	_nextScriptId = 1;
}

ScriptManager::~ScriptManager()
{
	_debugger->GetEmulator()->GetDebugHud()->ClearScreen();
	_debugger->GetEmulator()->GetScriptHud()->ClearScreen();
}

int ScriptManager::LoadScript(string name, string path, string content, int32_t scriptId)
{
	DebugBreakHelper helper(_debugger);
	auto lock = _scriptLock.AcquireSafe();

	if(scriptId < 0) {
		unique_ptr<ScriptHost> script(new ScriptHost(_nextScriptId++));
		script->LoadScript(name, path, content, _debugger);
		scriptId = script->GetScriptId();
		_scripts.push_back(std::move(script));
		_hasScript = true;
		//The callbacks it registered (or removed) as it loaded were counted before it was in _scripts
		RefreshMemoryCallbackFlags();
		return scriptId;
	} else {
		auto result = std::find_if(_scripts.begin(), _scripts.end(), [=](unique_ptr<ScriptHost>& script) {
			return script->GetScriptId() == scriptId;
		});
		if(result != _scripts.end()) {
			//Send a ScriptEnded event before reloading the code
			(*result)->ProcessEvent(EventType::ScriptEnded, _debugger->GetMainCpuType());

			(*result)->LoadScript(name, path, content, _debugger);
			RefreshMemoryCallbackFlags();
			return scriptId;
		}
	}

	return -1;
}

void ScriptManager::RemoveScript(int32_t scriptId)
{
	DebugBreakHelper helper(_debugger);
	auto lock = _scriptLock.AcquireSafe();
	auto predicate = [=](const unique_ptr<ScriptHost>& script) {
		if(script->GetScriptId() == scriptId) {
			//Send a ScriptEnded event before unloading the script
			script->ProcessEvent(EventType::ScriptEnded, _debugger->GetMainCpuType());
			_debugger->GetEmulator()->GetDebugHud()->ClearScreen();
			_debugger->GetEmulator()->GetScriptHud()->ClearScreen();
			_debugger->GetEmulator()->SetRenderOnDemand(false);
			return true;
		}
		return false;
	};

	_scripts.erase(std::remove_if(_scripts.begin(), _scripts.end(), predicate), _scripts.end());

	RefreshMemoryCallbackFlags();

	_hasScript = _scripts.size() > 0;
}

void ScriptManager::EnableCpuMemoryCallbacks(CpuType cpuType, CallbackType type)
{
	_cpuMemoryCallbackMask[(int)type] |= 1u << (int)cpuType;
	_debugger->SetScriptCallbackMask((int)type, _cpuMemoryCallbackMask[(int)type]);
}

void ScriptManager::RefreshMemoryCallbackFlags()
{
	_isPpuMemoryCallbackEnabled = false;
	for(int i = 0; i < 3; i++) {
		_cpuMemoryCallbackMask[i] = 0;
		_debugger->SetScriptCallbackMask(i, 0);
	}
	for(unique_ptr<ScriptHost>& script : _scripts) {
		script->RefreshMemoryCallbackFlags();
	}
	RefreshScriptPages();
}

void ScriptManager::RefreshScriptPages()
{
	for(int t = 0; t < 3; t++) {
		uint8_t pages[0x10000 / 8] = {};
		bool any = false;
		for(unique_ptr<ScriptHost>& script : _scripts) {
			ScriptingContext* context = script->GetContext();
			if(!context) {
				continue;
			}
			any |= context->MatchesAnyPage((CallbackType)t);
			const uint8_t* filter = context->GetPageFilter((CallbackType)t);
			for(int i = 0; i < 0x10000 / 8; i++) {
				pages[i] |= filter[i];
			}
		}
		_debugger->SetScriptPages(t, pages, any);
	}
}

string ScriptManager::GetScriptLog(int32_t scriptId)
{
	auto lock = _scriptLock.AcquireSafe();
	for(unique_ptr<ScriptHost>& script : _scripts) {
		if(script->GetScriptId() == scriptId) {
			return script->GetLog();
		}
	}
	return "";
}

void ScriptManager::ProcessEvent(EventType type, CpuType cpuType)
{
	for(unique_ptr<ScriptHost>& script : _scripts) {
		script->ProcessEvent(type, cpuType);
	}
}
