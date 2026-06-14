#!/bin/bash
# Smoke-test App Kit scripting via hey against real Cosmoe applications.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_SUBDIR="${COSMOE_BUILD_DIR:-builddir}"
BUILD_DIR="$SCRIPT_DIR/$BUILD_SUBDIR"
HEY="$BUILD_DIR/hey"
TEST_TIMEOUT="${COSMOE_SCRIPTING_TEST_TIMEOUT:-8s}"
START_TIMEOUT_SECONDS="${COSMOE_SCRIPTING_START_TIMEOUT:-10}"
KEEP_LOGS="${COSMOE_SCRIPTING_KEEP_LOGS:-0}"

if [[ ! -d "$BUILD_DIR" ]]; then
	echo "Error: Build directory $BUILD_DIR not found"
	echo "Set COSMOE_BUILD_DIR to the Meson build directory you want to test"
	exit 1
fi

if [[ ! -x "$HEY" ]]; then
	echo "Error: hey binary $HEY not found or not executable"
	echo "Please build the project first: ninja -C $BUILD_SUBDIR hey"
	exit 1
fi

if ! command -v timeout >/dev/null 2>&1; then
	echo "Error: timeout command is required"
	exit 1
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
	echo "No DISPLAY or WAYLAND_DISPLAY is set; scripting app smoke tests skipped"
	exit 77
fi

# Prefer just-built libraries over installed copies.
export LD_LIBRARY_PATH="$BUILD_DIR:$BUILD_DIR/src/kits${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

LOG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/cosmoe-scripting-tests.XXXXXX")"
launched_pids=()
passed=0
failed=0
skipped=0
launched_team=""

cleanup()
{
	for pid in "${launched_pids[@]}"; do
		if kill -0 "$pid" >/dev/null 2>&1; then
			timeout 3s "$HEY" "$pid" QUIT >/dev/null 2>&1 || kill "$pid" >/dev/null 2>&1 || true
		fi
	done

	for pid in "${launched_pids[@]}"; do
		wait "$pid" >/dev/null 2>&1 || true
	done

	if [[ "$KEEP_LOGS" != "1" ]]; then
		rm -rf "$LOG_DIR"
	else
		echo "Application logs kept in $LOG_DIR"
	fi
}
trap cleanup EXIT

record_pass()
{
	echo "PASS: $1"
	((passed++))
}

record_fail()
{
	echo "FAIL: $1"
	((failed++))
}

record_skip()
{
	echo "SKIP: $1"
	((skipped++))
}

run_hey()
{
	local output_var="$1"
	local team="$2"
	shift 2
	local command_output

	command_output="$(timeout "$TEST_TIMEOUT" "$HEY" "$team" "$@" 2>&1)"
	local status=$?
	printf -v "$output_var" '%s' "$command_output"
	return $status
}

expect_hey()
{
	local label="$1"
	local team="$2"
	local expected="$3"
	shift 3
	local output

	if ! run_hey output "$team" "$@"; then
		record_fail "$label"
		echo "$output" | sed 's/^/  /'
		return 1
	fi

	if [[ -n "$expected" ]] && ! grep -Eq "$expected" <<< "$output"; then
		record_fail "$label"
		echo "  Expected pattern: $expected"
		echo "$output" | sed 's/^/  /'
		return 1
	fi

	record_pass "$label"
	return 0
}

launch_app()
{
	local app_name="$1"
	local app_path="$BUILD_DIR/src/apps/$app_name"
	local log_path="$LOG_DIR/$app_name.log"
	launched_team=""

	if [[ ! -x "$app_path" ]]; then
		record_skip "$app_name executable not found"
		return 1
	fi

	"$app_path" >"$log_path" 2>&1 &
	local pid=$!
	launched_pids+=("$pid")

	local deadline=$((SECONDS + START_TIMEOUT_SECONDS))
	local output
	while (( SECONDS < deadline )); do
		if ! kill -0 "$pid" >/dev/null 2>&1; then
			record_fail "$app_name exited during startup"
			sed 's/^/  /' "$log_path"
			return 1
		fi

		if run_hey output "$pid" COUNT Window && grep -Eq '"result" \(B_INT32_TYPE\)' <<< "$output"; then
			launched_team="$pid"
			return 0
		fi

		sleep 0.2
	done

	record_fail "$app_name did not answer hey within ${START_TIMEOUT_SECONDS}s"
	sed 's/^/  /' "$log_path"
	return 1
}

test_common_app_suite()
{
	local app_name="$1"
	local team="$2"

	expect_hey "$app_name GETSUITES" "$team" 'B_REPLY' GETSUITES
	expect_hey "$app_name COUNT Window" "$team" '"result" \(B_INT32_TYPE\)' COUNT Window
}

test_window_suite()
{
	local app_name="$1"
	local team="$2"

	expect_hey "$app_name GET Title OF Window 0" "$team" '"result" \(B_STRING_TYPE\)' GET Title OF Window 0
	expect_hey "$app_name GET Frame OF Window 0" "$team" '"result" \(B_RECT_TYPE\) : BRect\(' GET Frame OF Window 0
}

test_set_title_suite()
{
	local app_name="$1"
	local team="$2"
	local title="Cosmoe scripting test - $app_name"

	expect_hey "$app_name SET Title OF Window 0" "$team" 'B_REPLY' SET Title OF Window 0 TO "$title"
	expect_hey "$app_name verify Title" "$team" "$title" GET Title OF Window 0
}

test_output_mode()
{
	local app_name="$1"
	local team="$2"
	local output

	if ! output="$(timeout "$TEST_TIMEOUT" "$HEY" -o "$team" GET Frame OF Window 0 2>&1)"; then
		record_fail "$app_name hey -o Frame"
		echo "$output" | sed 's/^/  /'
		return 1
	fi

	if ! grep -Eq '^BRect\(' <<< "$output"; then
		record_fail "$app_name hey -o Frame"
		echo "$output" | sed 's/^/  /'
		return 1
	fi

	record_pass "$app_name hey -o Frame"
}

run_app_tests()
{
	local app_name="$1"
	local mode="$2"
	local team

	echo ""
	echo "Running scripting tests for $app_name"
	echo "----------------------------------------"
	if ! launch_app "$app_name"; then
		return
	fi
	team="$launched_team"

	test_common_app_suite "$app_name" "$team"

	case "$mode" in
		full)
			test_window_suite "$app_name" "$team"
			test_set_title_suite "$app_name" "$team"
			test_output_mode "$app_name" "$team"
			;;
		window)
			test_window_suite "$app_name" "$team"
			;;
		app)
			;;
		*)
			record_fail "$app_name has unknown test mode '$mode'"
			;;
	esac
}

declare -A APP_MODES=(
	[Terminal]=full
	[StyledEdit]=window
	[DeskCalc]=window
)

if [[ -n "${COSMOE_SCRIPTING_APPS:-}" ]]; then
	read -r -a apps <<< "$COSMOE_SCRIPTING_APPS"
else
	apps=(Terminal StyledEdit DeskCalc)
fi

echo "Using build directory: $BUILD_DIR"
echo "Using hey: $HEY"
echo "Using logs: $LOG_DIR"

for app in "${apps[@]}"; do
	run_app_tests "$app" "${APP_MODES[$app]:-window}"
done

echo ""
echo "Scripting test summary"
echo "----------------------"
echo "Passed:  $passed"
echo "Failed:  $failed"
echo "Skipped: $skipped"

if (( failed > 0 )); then
	exit 1
fi

exit 0