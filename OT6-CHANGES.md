# Changes in this branch

This branch (`ot6`) is a modified version of MesenCE 2.2.1
(github.com/nesdev-org/MesenCE, tag `2.2.1`, commit `20ba206c`), itself
a community fork of Mesen (github.com/SourMesen/Mesen2). It was modified by
Mike Klein for the OT6 project (a Final Fantasy VI ROM hack, whose test
harness drives Mesen through Lua) from 2026-10-01. Like Mesen, it is
licensed under the GNU General Public License v3 (LICENSE).

The changes, one commit each:

- **Script-only debugger mode** (Core/Debugger, Core/SNES/Debugger). With
  `MESEN_SCRIPT_ONLY=1` in the environment and no debugger window open,
  the SNES CPU, SA-1 and SPC debuggers skip the per-access bookkeeping that
  only the debugger windows read (access counters, code/data log, call
  stack, event log), while Lua callbacks, breakpoints, stepping and break
  requests still work. Two always-on changes make Lua memory-callback
  dispatch cheaper without changing which callbacks run.
- **Screenshot sync** (Core/Shared/Video/VideoDecoder.cpp).
  `emu.takeScreenshot()` waits for the pending frame decode, so it returns
  the frame just finished instead of sometimes the one before.
- **Render on demand, and a debugger that pays only for the script's
  callbacks** (Core/SNES/SnesPpu.cpp, Core/Shared/Emulator.h,
  Core/Debugger; 2026-10-05). Lua's `emu.setRenderOnDemand(true)` makes the
  SNES PPU draw only the frames a script asks for with
  `emu.requestRender()`, through Mesen's own frame-skip path, so emulation
  is unchanged; `emu.isFrameRendered()` says whether the frame just
  finished was drawn. Off by default, and unloading the script turns it
  off. In script-only mode the debugger's hot paths (memory accesses,
  instructions, idle and PPU cycles) test inline whether the CPU is quiet
  and a script callback could run, counting callbacks per type and per
  256-byte page, and call into the debugger only then; nothing that runs
  changes. A frame not drawn still evaluates (layers and backdrop, no
  output) its last visible line, every line of a frame whose HDMA writes
  INIDISP, the line where forced blank turned on the frame before, and the
  current line when forced blank turns on, so the palette-lookup address
  (InternalCgramAddress) a drawn frame leaves is the same; a savestate or
  a CGRAM access during rendering while it may not be is counted
  (`emu.getRenderOnDemandInexact()`).
- **SDK roll-forward** (UI/global.json). The .NET SDK may roll forward to
  a newer major version, so the .NET 10 SDK can build it; the UI still
  targets .NET 8.

Without `MESEN_SCRIPT_ONLY=1` the program behaves like MesenCE 2.2.1 apart
from the cheaper callback dispatch, the screenshot sync and the new Lua
calls (inert until a script calls them). The commit
messages give the details. The same changes on Mesen 2.1.1 are tagged
`ot6-2.1.1-1`.
