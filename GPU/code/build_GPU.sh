#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

RESULT_DIR="gpu_test_results"
mkdir -p "$RESULT_DIR"

SUMMARY_FILE="$RESULT_DIR/summary.txt"
: > "$SUMMARY_FILE"

if [ ! -f "./Rockyou-singleLined-full.txt" ]; then
    echo "ERROR: dataset ./Rockyou-singleLined-full.txt not found." | tee -a "$SUMMARY_FILE"
    echo "Please put Rockyou-singleLined-full.txt in the code directory."
    exit 1
fi

if [ -n "${HIPCC:-}" ]; then
    HIPCC_BIN="$HIPCC"
elif command -v hipcc >/dev/null 2>&1; then
    HIPCC_BIN="$(command -v hipcc)"
elif [ -x /opt/rocm/bin/hipcc ]; then
    HIPCC_BIN="/opt/rocm/bin/hipcc"
else
    {
        echo "ERROR: hipcc was not found."
        echo "Exit code 127 usually means a command is missing."
        echo "Try one of these on the AMD platform:"
        echo "  which hipcc"
        echo "  ls /opt/rocm/bin/hipcc"
        echo "  export PATH=/opt/rocm/bin:\$PATH"
    } | tee -a "$SUMMARY_FILE"
    exit 127
fi

echo "Using HIP compiler: $HIPCC_BIN" | tee -a "$SUMMARY_FILE"
echo | tee -a "$SUMMARY_FILE"

for OPT in O0 O1 O2; do
    EXE="main_GPU_${OPT}"
    RESULT_FILE="$RESULT_DIR/result_GPU_${OPT}.txt"

    {
        echo "========================================"
        echo "GPU PCFG test with -${OPT}"
        echo "Work directory: $SCRIPT_DIR"
        echo "Executable: $EXE"
        echo "Dataset: ./Rockyou-singleLined-full.txt"
        echo "Compile command:"
        echo "$HIPCC_BIN main_GPU.cpp train.cpp guessing_GPU.hip md5.cpp -o ${EXE} -${OPT}"
        echo "========================================"
        echo
    } > "$RESULT_FILE"

    "$HIPCC_BIN" main_GPU.cpp train.cpp guessing_GPU.hip md5.cpp -o "$EXE" "-${OPT}" >> "$RESULT_FILE" 2>&1

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
        echo "[-${OPT}]"
        grep -E "Guess time:|Hash time:|Train time:" "$RESULT_FILE" || true
        echo "Result file: $RESULT_FILE"
        echo
    } >> "$SUMMARY_FILE"
done

echo "All GPU tests finished."
echo "Results are saved in: $RESULT_DIR"
echo "Summary: $SUMMARY_FILE"
