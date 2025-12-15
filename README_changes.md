# Change Summary (Minimal – Steps 1 to 4)

This document summarizes the **minimal changes** applied to the original `debugger.c`.

---

## Step 1: Basic Error Handling

**Change**
Critical system calls (`ptrace`, `wait`, `execl`) and user input are now checked for failure using a small `CHECK()` macro and simple validation.

**Reason**
The lab requires graceful error handling. Without checks, failures could lead to undefined debugger behavior.

---

## Step 2: Explicit Process Status Display

**Change**
A small helper function was added to print whether the debuggee is stopped, exited, or terminated after each `wait()`.

**Reason**
this was required to be made.
---

## Step 3: Corrected Single-Step Loop

**Change**
The original `while (WIFSTOPPED(status))` loop was replaced with a loop that reevaluates `status` after each `wait()`.

**Reason**
The original logic reused a stale `wait()` status, which could cause incorrect stepping after process exit.

---

## Step 4: Explicit Breakpoint Removal

**Change**
Breakpoint removal is now explicitly performed and logged by restoring the original instruction before resuming execution.

**Reason**
The lab requires both setting and removing software breakpoints as explicit debugger actions.

---

## Scope Control

No additional features were added. The following were intentionally left unchanged:

* Single-breakpoint design
* Manual address input
* ARM64-specific implementation
* Minimal user interface

