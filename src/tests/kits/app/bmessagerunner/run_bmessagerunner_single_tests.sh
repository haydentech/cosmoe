#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../../" && pwd)
BUILD_DIR="${PROJECT_ROOT}/builddir"
BIN_PATH="${BUILD_DIR}/src/tests/kits/app/bmessagerunner_single"
OUT_DIR="${BUILD_DIR}/test-results/bmessagerunner"
mkdir -p "${OUT_DIR}"

UNIT_TEST_TIMEOUT=${UNIT_TEST_TIMEOUT:-15}
echo "Running BMessageRunner tests with timeout=${UNIT_TEST_TIMEOUT}s"

# Build binary if not built
if [ ! -x "${BIN_PATH}" ]; then
  echo "Building bmessagerunner_single..."
  ninja -C "${BUILD_DIR}" src/tests/kits/app/bmessagerunner_single
fi

tests=(A1 A2 A3 A4 A5 A6 A7 A8 B1 B2 B3 B4 B5 B6 B7 B8 B9)
failures=()

for t in "${tests[@]}"; do
  logfile="${OUT_DIR}/${t}.log"
  echo "Running test ${t}..."
  # run the test in its own process with a timeout
  if timeout -s SIGKILL ${UNIT_TEST_TIMEOUT}s "${BIN_PATH}" "${t}" > "${logfile}" 2>&1; then
    echo "${t}: PASS"
  else
    rc=$?
    echo "${t}: FAIL (rc=${rc}) - see ${logfile}"
    failures+=("${t}")
  fi
done

echo -e "\nTest summary for BMessageRunner:"
for t in "${tests[@]}"; do
  if grep -q "READY_TO_ATTACH" "${OUT_DIR}/${t}.log"; then
    echo -n "${t}: READY_TO_ATTACH "
  fi
  grep -n "\[msg runner" "${OUT_DIR}/${t}.log" || true
done

if [ ${#failures[@]} -ne 0 ]; then
  echo -e "\nSome tests failed: ${failures[*]}"
  exit 1
else
  echo -e "\nAll tests passed."
fi
