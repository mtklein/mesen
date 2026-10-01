# Changes in this branch

This branch (`ot6`) is a modified version of Mesen 2.1.1
(github.com/SourMesen/Mesen2, tag `2.1.1`, commit `137ae7ce`). It was
modified by Mike Klein for the OT6 project (a Final Fantasy VI ROM hack,
whose test harness drives Mesen through Lua) on 2026-10-01. Like Mesen,
it is licensed under the GNU General Public License v3 (LICENSE).

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

Without `MESEN_SCRIPT_ONLY=1` the program behaves like Mesen 2.1.1 apart
from the cheaper callback dispatch and the screenshot sync. The commit
messages give the details.
