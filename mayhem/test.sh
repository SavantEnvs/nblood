#!/usr/bin/env bash
#
# mayhem/test.sh — run the known-answer probe built by mayhem/build.sh (normal flags, dynamically linked).
# It decodes fixed PNG/BMP/ZIP/script/INI/JSON inputs through NBlood's real parsers and asserts exact values.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

EXPECTED=14   # number of `KAT ok` lines kat_probe must print

if [ ! -x /mayhem/kat_probe ]; then
  echo "kat_probe missing — mayhem/build.sh should have built it" >&2
  emit_ctrf "nblood-kat" 0 1 0
  exit $?
fi

out="$(/mayhem/kat_probe mayhem/kat/fixtures 2>&1)"
printf '%s\n' "$out"

passed=$(printf '%s\n' "$out" | grep -c '^KAT ok ')
failed=$(printf '%s\n' "$out" | grep -c '^KAT FAIL ')
# a probe that printed nothing (or fewer checks than exist) is a failure, never a skip
missing=$(( EXPECTED - passed - failed ))
[ "$missing" -gt 0 ] && failed=$(( failed + missing ))
printf '%s\n' "$out" | grep -q "^KAT_SUMMARY passed=$EXPECTED failed=0\$" || [ "$failed" -gt 0 ] || failed=1

emit_ctrf "nblood-kat" "$passed" "$failed" 0
