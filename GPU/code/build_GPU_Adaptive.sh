#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

RESULT_DIR="gpu_adaptive_test_results"
mkdir -p "$RESULT_DIR"

SUMMARY_FILE="$RESULT_DIR/summary_adaptive.txt"
: > "$SUMMARY_FILE"

if [ ! -f "./Rockyou-singleLined-full.txt" ]; then
    echo "ERROR: dataset ./Rockyou-singleLined-full.txt not found." | tee -a "$SUMMARY_FILE"
    exit 1
fi

if [ -n "${HIPCC:-}" ]; then
    HIPCC_BIN="$HIPCC"
elif command -v hipcc >/dev/null 2>&1; then
    HIPCC_BIN="$(command -v hipcc)"
elif [ -x /opt/rocm/bin/hipcc ]; then
    HIPCC_BIN="/opt/rocm/bin/hipcc"
else
    echo "ERROR: hipcc was not found." | tee -a "$SUMMARY_FILE"
    exit 127
fi

echo "Using HIP compiler: $HIPCC_BIN" | tee -a "$SUMMARY_FILE"
echo | tee -a "$SUMMARY_FILE"

THRESHOLDS="${THRESHOLDS:-1024 8192 32768}"

for OPT in O0 O1 O2; do
  for THRESHOLD in $THRESHOLDS; do
    EXE="main_GPU_Adaptive_${OPT}_T${THRESHOLD}"
    RESULT_FILE="$RESULT_DIR/result_GPU_Adaptive_${OPT}_T${THRESHOLD}.txt"

    {
        echo "========================================"
        echo "Adaptive GPU PCFG test with -${OPT}"
        echo "GPU threshold: ${THRESHOLD}"
        echo "Work directory: $SCRIPT_DIR"
        echo "Executable: $EXE"
        echo "Dataset: ./Rockyou-singleLined-full.txt"
        echo "Compile command:"
        echo "$HIPCC_BIN main_GPU_Adaptive.cpp train.cpp guessing_GPU_Adaptive.hip md5.cpp -o ${EXE} -${OPT} -DGPU_PT_THRESHOLD=${THRESHOLD}"
        echo "========================================"
        echo
    } > "$RESULT_FILE"

    "$HIPCC_BIN" main_GPU_Adaptive.cpp train.cpp guessing_GPU_Adaptive.hip md5.cpp -o "$EXE" "-${OPT}" "-DGPU_PT_THRESHOLD=${THRESHOLD}" >> "$RESULT_FILE" 2>&1

    {
        echo
        echo "--------------- Run output ---------------"
        if [ -x /usr/bin/time ]; then
            /usr/bin/time -p "./${EXE}"
        else
            time "./${EXE}"
        fi
        echo "--------------- End output ---------------"
    } >> "$RESULT_FILE" 2>&1

    {
        echo "[-${OPT}, threshold=${THRESHOLD}]"
        grep -E "Guess time:|Hash time:|Train time:|Adaptive threshold:|Adaptive CPU PTs:|Adaptive GPU PTs:|Adaptive CPU guesses:|Adaptive GPU guesses:" "$RESULT_FILE" || true
        echo "Result file: $RESULT_FILE"
        echo
    } >> "$SUMMARY_FILE"
  done
done

echo "All adaptive GPU tests finished."
echo "Results are saved in: $RESULT_DIR"
echo "Summary: $SUMMARY_FILE"
