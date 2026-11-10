#!/usr/bin/env bash
# GDB holds actual production admission, not a substitute for same-FD3 A0.
set -euo pipefail
cd "$(dirname "$0")/../.."
: "${SAIVAGE_CARD_WORK_ROOT:?}" "${DOSBOX_TEST_BINARY:?}"
binary=$(realpath "$DOSBOX_TEST_BINARY")
work="$SAIVAGE_CARD_WORK_ROOT/control-native"
mkdir -p "$work"
pending=$(grep -n 'local_driver_preparing=true;' src/debug/debug.cpp | cut -d: -f1)
cat > "$work/probe.gdb" <<EOF
set pagination off
set confirm off
break $PWD/src/debug/debug.cpp:$pending
commands
silent
if local_driver_operation == 1
set native_control.request_.attachment.serial = local_driver_attachment.serial
set native_control.request_.operation = local_driver_operation
set native_control.request_.kind = NativeDebug::Control::Kind::Pause
printf "S0A Pending before/consume/after\\n"
print native_execution.state_
print DEBUG_ConsumeNativeControl(native_control.request_)
print native_execution.state_
end
if local_driver_operation >= 3
enable 2
disable 1
end
continue
end
break LocalDriverFrontier
condition 2 native_execution.state_ == NativeDebug::State::Active
commands
silent
set native_control.request_.attachment.serial = local_driver_attachment.serial
set native_control.request_.operation = 1
set native_control.request_.kind = NativeDebug::Control::Kind::Pause
printf "S0A stale against active successor\\n"
print native_execution.result_.operation
print DEBUG_ConsumeNativeControl(native_control.request_)
print native_execution.state_
set native_control.request_.operation = local_driver_operation
printf "S0A exact Active\\n"
print DEBUG_ConsumeNativeControl(native_control.request_)
print CPU_Cycles
print exitLoop
disable 2
continue
end
disable 2
run
EOF
printf '#!/bin/sh\nexec gdb -q -batch -x "%s/probe.gdb" --args "%s" "$@"\n' "$work" "$binary" > "$work/wrapper"
chmod +x "$work/wrapper"
DOSBOX_TEST_BINARY="$work/wrapper" python3 tests/machine-service/lua-production-runtime.py | tee "$work/result.log"
run=$(head -n1 "$work/result.log")
grep -aF 'result = NativeDebug::ControlResult::Pending' "$run/pty.log"
grep -aF 'result = NativeDebug::ControlResult::Unknown' "$run/pty.log"
grep -aF 'result = NativeDebug::ControlResult::StopRequested' "$run/pty.log"
grep -F 'run manual' "$work/result.log"
echo 'Production native held Pending / stale successor / Active Pause PASS (GDB-held; not FD3)'
