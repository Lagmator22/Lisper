#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-$ROOT_DIR/build/lisper}"
MODEL="${MODEL:-$ROOT_DIR/models/ggml-base.en.bin}"

if [[ ! -x "$BIN" ]]; then
  echo "Binary not found or not executable: $BIN"
  exit 1
fi

if [[ ! -f "$MODEL" ]]; then
  echo "Model not found: $MODEL"
  exit 1
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/lisper-stress.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

mkdir -p "$WORK_DIR/in" "$WORK_DIR/out" "$WORK_DIR/logs"
cp "$ROOT_DIR/jfk.wav" "$WORK_DIR/in/jfk.wav"
cp "$ROOT_DIR/test.aiff" "$WORK_DIR/in/test.aiff"

case_index=0
pass_count=0
fail_count=0

run_cmd_case() {
  local title="$1"
  local expected_rc="$2"
  local cmd="$3"
  local log_file="$WORK_DIR/logs/case_${case_index}.log"

  case_index=$((case_index + 1))

  bash -lc "$cmd" >"$log_file" 2>&1
  local rc=$?

  if [[ "$rc" -eq "$expected_rc" ]]; then
    pass_count=$((pass_count + 1))
    printf "[%03d] PASS %-40s (rc=%d)\n" "$case_index" "$title" "$rc"
  else
    fail_count=$((fail_count + 1))
    printf "[%03d] FAIL %-40s (rc=%d expected=%d)\n" \
      "$case_index" "$title" "$rc" "$expected_rc"
    tail -n 12 "$log_file" | sed 's/^/      /'
  fi
}

run_watch_case() {
  local title="$1"
  local log_file="$WORK_DIR/logs/case_${case_index}.log"

  case_index=$((case_index + 1))
  local watch_in="$WORK_DIR/watch_in_${case_index}"
  local watch_out="$WORK_DIR/watch_out_${case_index}"
  mkdir -p "$watch_in" "$watch_out"

  "$BIN" -m "$MODEL" -w "$watch_in" -o "$watch_out" -f text \
    >"$log_file" 2>&1 &
  local pid=$!

  sleep 1
  cp "$ROOT_DIR/jfk.wav" "$watch_in/case_${case_index}.wav"
  sleep 1
  kill -INT "$pid" >/dev/null 2>&1
  wait "$pid"
  local rc=$?

  if [[ "$rc" -eq 130 ]]; then
    pass_count=$((pass_count + 1))
    printf "[%03d] PASS %-40s (rc=%d)\n" "$case_index" "$title" "$rc"
  else
    fail_count=$((fail_count + 1))
    printf "[%03d] FAIL %-40s (rc=%d expected=130)\n" \
      "$case_index" "$title" "$rc"
    tail -n 12 "$log_file" | sed 's/^/      /'
  fi
}

echo "Running 100 stress cases in $WORK_DIR"

# 1-20: CLI parsing and validation
run_cmd_case "help short" 0 "'$BIN' -h"
run_cmd_case "help long" 0 "'$BIN' --help"
run_cmd_case "list model profiles" 0 "'$BIN' --list-model-profiles"
run_cmd_case "missing model and input" 1 "'$BIN' --device cpu '$WORK_DIR/in/jfk.wav'"
run_cmd_case "unknown model profile" 1 "'$BIN' --model-profile invalid '$WORK_DIR/in/jfk.wav'"
run_cmd_case "invalid model path" 1 "'$BIN' -m '$WORK_DIR/in/nope.bin' '$WORK_DIR/in/jfk.wav'"
run_cmd_case "invalid file path" 1 "'$BIN' -m '$MODEL' '$WORK_DIR/in/nope.wav'"
run_cmd_case "invalid thread string" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -t abc -o '$WORK_DIR/out/t1.txt'"
run_cmd_case "invalid thread zero" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -t 0 -o '$WORK_DIR/out/t2.txt'"
run_cmd_case "invalid gpu device" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' --gpu-device nope -o '$WORK_DIR/out/t3.txt'"
run_cmd_case "invalid device fallback" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' --device something -o '$WORK_DIR/out/t4.txt'"
run_cmd_case "single text default" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -o '$WORK_DIR/out/t5.txt'"
run_cmd_case "single json" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -f json -o '$WORK_DIR/out/t6.json'"
run_cmd_case "single srt" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -f srt -o '$WORK_DIR/out/t7.srt'"
run_cmd_case "single rag" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -f rag -o '$WORK_DIR/out/t8.txt'"
run_cmd_case "cpu mode" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' --device cpu -o '$WORK_DIR/out/t9.txt'"
run_cmd_case "auto mode" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' --device auto -o '$WORK_DIR/out/t10.txt'"
run_cmd_case "gpu mode" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' --device gpu -o '$WORK_DIR/out/t11.txt'"
run_cmd_case "no color" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' --no-color -o '$WORK_DIR/out/t12.txt'"
run_cmd_case "no animate" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' --no-animate -o '$WORK_DIR/out/t13.txt'"

# 21-60: 40-case transcription matrix
formats=(text json srt rag)
threads=(1 2 4 6 8)
devices=(auto cpu)
for fmt in "${formats[@]}"; do
  for th in "${threads[@]}"; do
    for dev in "${devices[@]}"; do
      run_cmd_case "matrix fmt=$fmt th=$th dev=$dev" 0 \
        "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -f $fmt -t $th --device $dev -o '$WORK_DIR/out/matrix_${fmt}_${th}_${dev}'"
    done
  done
done

# 61-75: path and media edge cases
cp "$ROOT_DIR/jfk.wav" "$WORK_DIR/in/with space.wav"
cp "$ROOT_DIR/jfk.wav" "$WORK_DIR/in/with'quote.wav"
cp "$ROOT_DIR/jfk.wav" "$WORK_DIR/in/with\$money.wav"

run_cmd_case "space in input path" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/with space.wav' -o '$WORK_DIR/out/p1.txt'"
run_cmd_case "quote in input path" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/with'\\''quote.wav' -o '$WORK_DIR/out/p2.txt'"
run_cmd_case "dollar in input path" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/with\$money.wav' -o '$WORK_DIR/out/p3.txt'"
run_cmd_case "aiff conversion path" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/test.aiff' -o '$WORK_DIR/out/p4.txt'"
run_cmd_case "aiff conversion json" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/test.aiff' -f json -o '$WORK_DIR/out/p5.json'"
run_cmd_case "aiff conversion rag" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/test.aiff' -f rag -o '$WORK_DIR/out/p6.txt'"
run_cmd_case "write denied 1" 1 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -o '/System/lisper_denied_1.txt'"
run_cmd_case "write denied 2" 1 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -o '/System/lisper_denied_2.txt'"
run_cmd_case "write denied 3" 1 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -o '/System/lisper_denied_3.txt'"
run_cmd_case "write denied 4" 1 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -o '/System/lisper_denied_4.txt'"
run_cmd_case "write denied 5" 1 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -o '/System/lisper_denied_5.txt'"
run_cmd_case "model profile with -m override" 0 "'$BIN' --model-profile quality -m '$MODEL' '$WORK_DIR/in/jfk.wav' -o '$WORK_DIR/out/p7.txt'"
run_cmd_case "flash attn off" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' --no-flash-attn -o '$WORK_DIR/out/p8.txt'"
run_cmd_case "translate flag" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' --translate -o '$WORK_DIR/out/p9.txt'"
run_cmd_case "language fr" 0 "'$BIN' -m '$MODEL' '$WORK_DIR/in/jfk.wav' -l fr -o '$WORK_DIR/out/p10.txt'"

# 76-90: batch mode stress
for i in {1..5}; do
  mkdir -p "$WORK_DIR/batch_${i}"
  cp "$ROOT_DIR/jfk.wav" "$WORK_DIR/batch_${i}/a.wav"
  cp "$ROOT_DIR/jfk.wav" "$WORK_DIR/batch_${i}/b.wav"
done

run_cmd_case "batch text auto" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_1' -o '$WORK_DIR/out/b1' -f text"
run_cmd_case "batch json auto" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_2' -o '$WORK_DIR/out/b2' -f json"
run_cmd_case "batch srt auto" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_3' -o '$WORK_DIR/out/b3' -f srt"
run_cmd_case "batch rag auto" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_4' -o '$WORK_DIR/out/b4' -f rag"
run_cmd_case "batch cpu mode" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_5' -o '$WORK_DIR/out/b5' --device cpu"
run_cmd_case "batch missing dir" 1 "'$BIN' -m '$MODEL' -d '$WORK_DIR/no_batch' -o '$WORK_DIR/out/b6'"
run_cmd_case "batch missing output file path err" 1 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_1' -o '/System/lisper_batch_denied'"
run_cmd_case "batch no output path" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_1'"
run_cmd_case "batch threads 2" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_2' -o '$WORK_DIR/out/b7' -t 2"
run_cmd_case "batch threads 8" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_3' -o '$WORK_DIR/out/b8' -t 8"
run_cmd_case "batch translate" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_4' -o '$WORK_DIR/out/b9' --translate"
run_cmd_case "batch language es" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_5' -o '$WORK_DIR/out/b10' -l es"
run_cmd_case "batch no-color" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_1' -o '$WORK_DIR/out/b11' --no-color"
run_cmd_case "batch no-animate" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_2' -o '$WORK_DIR/out/b12' --no-animate"
run_cmd_case "batch device gpu" 0 "'$BIN' -m '$MODEL' -d '$WORK_DIR/batch_3' -o '$WORK_DIR/out/b13' --device gpu"

# 91-100: watch mode + interrupt stress
for i in {1..10}; do
  run_watch_case "watch interrupt $i"
done

echo
echo "Stress summary: total=$case_index pass=$pass_count fail=$fail_count"

if [[ "$case_index" -ne 100 ]]; then
  echo "Expected 100 cases, got $case_index"
  exit 1
fi

if [[ "$fail_count" -ne 0 ]]; then
  exit 1
fi

exit 0
