#!/bin/bash

# ==============================================================================
# Script Name: run_perf_tests.sh
# Description: Executes multiple programs with varying configurations,
#              collects performance metrics using 'perf', and logs the data
#              into a CSV file.
# Author: Anindit Bordoloi
# ==============================================================================

# ==============================================================================
# Configuration Variables
# ==============================================================================

# Number of iterations per configuration
ITERATIONS=3

# Output files
OUTPUT_FILE="perf_data.csv"
ERROR_LOG="error.log"
LOG_FILE="perf_run.log"

# ==============================================================================
# Redirect All Output to Log File
# ==============================================================================

# Redirect both stdout and stderr to LOG_FILE while displaying in terminal
exec > >(tee -a "$LOG_FILE") 2>&1

# ==============================================================================
# Backup Existing Output File
# ==============================================================================

# Create a backup of the existing CSV file to prevent data loss
if [ -f "$OUTPUT_FILE" ]; then
    BACKUP_FILE="${OUTPUT_FILE}.bak_$(date +%F_%T)"
    cp "$OUTPUT_FILE" "$BACKUP_FILE"
    echo "Backup of existing output file created as $BACKUP_FILE"
fi

# ==============================================================================
# Define Programs and Their Configurations
# ==============================================================================

# Define all programs and their specific data sizes
declare -A PROGRAMS=(
    ["./mc_31"]="100000 200000 300000 400000 500000"
    ["./sc_28"]="1000000000 2000000000 3000000000 4000000000 5000000000"
    ["./sc_29"]="100000000 200000000 300000000 400000000 500000000"
    ["./seq_10"]="1000000000 2000000000 3000000000 4000000000 5000000000"
    ["./vec_14"]="100000000 100000000 300000000 400000000 500000000"
    ["./vec_23"]="100000000 100000000 300000000 400000000 500000000"
    # Add more programs and their data sizes here if needed
)

# Define modes for each program
declare -A PROGRAM_MODES=(
    ["./mc_31"]="good bad-fs bad-ma"
    ["./sc_28"]="good bad-fs bad-ma"
    ["./sc_29"]="good bad-fs bad-ma"
    ["./seq_10"]="good bad"
    ["./vec_14"]="good bad-fs bad-ma"
    ["./vec_23"]="good bad-fs bad-ma"
    # Add more programs and their modes here if needed
)

# Define thread counts for each program
declare -A PROGRAM_THREADS=(
    ["./mc_31"]="1 2 3 4 5 6 7 8"
    ["./sc_28"]="1 2 3 4 5 6 7 8"
    ["./sc_29"]="1 2 3 4 5 6 7 8"
    ["./seq_10"]="1 2 3 4 5 6 7 8"
    ["./vec_14"]="1 2 3 4 5 6 7 8"
    ["./vec_23"]="1 2 3 4 5 6 7 8"
    # Add more programs and their thread counts here if needed
)

# ==============================================================================
# Function: write_header
# Description: Writes the CSV header if the output file does not exist.
# ==============================================================================
write_header() {
    echo "Program,Mode,Threads,Data_Size,Run,cache_references,cache_misses,L1_dcache_loads,L1_dcache_load_misses,L1_dcache_prefetches,dTLB_loads,dTLB_load_misses,branch_instructions,branch_misses,context_switches,cpu_migrations,stalled_cycles_backend,stalled_cycles_frontend,cpu_cycles,instructions,elapsed_time,user_time,sys_time" > "$OUTPUT_FILE"
    echo "Created new output file and added header: $OUTPUT_FILE"
}

# ==============================================================================
# Initialize Output CSV File
# ==============================================================================
if [ ! -f "$OUTPUT_FILE" ]; then
    write_header
else
    echo "Output file exists. Appending data to $OUTPUT_FILE"
fi

# ==============================================================================
# Function: extract_metrics
# Description: Parses 'perf' output and extracts relevant performance metrics.
# ==============================================================================
extract_metrics() {
    local perf_output="$1"

    # Use awk to parse the perf output
    echo "$perf_output" | awk '
    BEGIN {
        FS=" ";
        OFS=",";
        # Initialize all variables to "0" to handle cases where metrics are missing
        cache_references = cache_misses = L1_dcache_loads = L1_dcache_load_misses = L1_dcache_prefetches = dTLB_loads = dTLB_load_misses = branch_instructions = branch_misses = context_switches = cpu_migrations = stalled_cycles_backend = stalled_cycles_frontend = cpu_cycles = instructions = elapsed_time = user_time = sys_time = "0";
    }
    /cache-references/ {cache_references=$1}
    /cache-misses/ {cache_misses=$1}
    /L1-dcache-loads/ {L1_dcache_loads=$1}
    /L1-dcache-load-misses/ {L1_dcache_load_misses=$1}
    /L1-dcache-prefetches/ {L1_dcache_prefetches=$1}
    /dTLB-loads/ {dTLB_loads=$1}
    /dTLB-load-misses/ {dTLB_load_misses=$1}
    /branch-instructions/ {branch_instructions=$1}
    /branch-misses/ {branch_misses=$1}
    /context-switches/ {context_switches=$1}
    /cpu-migrations/ {cpu_migrations=$1}
    /stalled-cycles-backend/ {stalled_cycles_backend=$1}
    /stalled-cycles-frontend/ {stalled_cycles_frontend=$1}
    /cycles / {cpu_cycles=$1}
    /instructions / {instructions=$1}
    /seconds time elapsed/ {elapsed_time=$1}
    /seconds user/ {user_time=$1}
    /seconds sys/ {sys_time=$1}
    END {
        # Replace <not counted> with 0
        gsub(/<not counted>/, "0", cache_references);
        gsub(/<not counted>/, "0", cache_misses);
        gsub(/<not counted>/, "0", L1_dcache_loads);
        gsub(/<not counted>/, "0", L1_dcache_load_misses);
        gsub(/<not counted>/, "0", L1_dcache_prefetches);
        gsub(/<not counted>/, "0", dTLB_loads);
        gsub(/<not counted>/, "0", dTLB_load_misses);
        gsub(/<not counted>/, "0", branch_instructions);
        gsub(/<not counted>/, "0", branch_misses);
        gsub(/<not counted>/, "0", context_switches);
        gsub(/<not counted>/, "0", cpu_migrations);
        gsub(/<not counted>/, "0", stalled_cycles_backend);
        gsub(/<not counted>/, "0", stalled_cycles_frontend);
        gsub(/<not counted>/, "0", cpu_cycles);
        gsub(/<not counted>/, "0", instructions);
        gsub(/<not counted>/, "0", elapsed_time);
        gsub(/<not counted>/, "0", user_time);
        gsub(/<not counted>/, "0", sys_time);

        # Remove commas from all numeric values to prevent CSV cell splitting
        gsub(/,/, "", cache_references);
        gsub(/,/, "", cache_misses);
        gsub(/,/, "", L1_dcache_loads);
        gsub(/,/, "", L1_dcache_load_misses);
        gsub(/,/, "", L1_dcache_prefetches);
        gsub(/,/, "", dTLB_loads);
        gsub(/,/, "", dTLB_load_misses);
        gsub(/,/, "", branch_instructions);
        gsub(/,/, "", branch_misses);
        gsub(/,/, "", context_switches);
        gsub(/,/, "", cpu_migrations);
        gsub(/,/, "", stalled_cycles_backend);
        gsub(/,/, "", stalled_cycles_frontend);
        gsub(/,/, "", cpu_cycles);
        gsub(/,/, "", instructions);
        gsub(/,/, "", elapsed_time);
        gsub(/,/, "", user_time);
        gsub(/,/, "", sys_time);

        print cache_references, cache_misses, L1_dcache_loads, L1_dcache_load_misses, L1_dcache_prefetches, dTLB_loads, dTLB_load_misses, branch_instructions, branch_misses, context_switches, cpu_migrations, stalled_cycles_backend, stalled_cycles_frontend, cpu_cycles, instructions, elapsed_time, user_time, sys_time;
    }'
}

# ==============================================================================
# Function: run_perf_and_log
# Description: Executes a program with given parameters, collects perf metrics,
#              and logs the results into the CSV file.
# ==============================================================================
run_perf_and_log() {
    local program="$1"
    local mode="$2"
    local data_size="$3"
    local threads="$4"
    local run="$5"

    echo "    Run #$run"
    echo "    Executing: $program $mode $data_size $threads"

    # Execute the program with current configuration and capture perf output
    PERF_OUTPUT=$(perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-dcache-prefetches,dTLB-loads,dTLB-load-misses,branch-instructions,branch-misses,context-switches,cpu-migrations,stalled-cycles-backend,stalled-cycles-frontend,cpu-cycles,instructions \
        "$program" "$mode" "$data_size" "$threads" 2>&1)

    # Check if the program executed successfully
    if [ $? -ne 0 ]; then
        echo "Error: Program $program encountered an error during execution."
        # Log the error in the CSV with an ERROR flag and empty fields for metrics
        LINE="$program,$mode,$threads,$data_size,$run,ERROR,,,,,,,,,,,,,,,"
        echo "$LINE" >> "$OUTPUT_FILE"
        echo "Error during run #$run of program $program with Mode=$mode, Threads=$threads, Data_Size=$data_size" >> "$ERROR_LOG"
        return 1
    fi

    # Extract metrics from perf output
    METRICS=$(extract_metrics "$PERF_OUTPUT")

    # Combine all extracted metrics into a single line
    LINE="$program,$mode,$threads,$data_size,$run,$METRICS"

    # Append the line to the output CSV file
    echo "$LINE" >> "$OUTPUT_FILE"
}

# ==============================================================================
# Verify Executability of All Programs
# ==============================================================================
for PROGRAM in "${!PROGRAMS[@]}"; do
    if [ ! -x "$PROGRAM" ]; then
        echo "Error: Program $PROGRAM not found or not executable."
        exit 1
    fi
done

# ==============================================================================
# Main Execution Loop
# ==============================================================================
echo "Starting performance tests for all programs."

for PROGRAM in "${!PROGRAMS[@]}"; do
    DATA_SIZES=(${PROGRAMS[$PROGRAM]})
    MODES=(${PROGRAM_MODES[$PROGRAM]})
    THREADS=(${PROGRAM_THREADS[$PROGRAM]})

    echo "Starting performance tests for program: $PROGRAM"

    for MODE in "${MODES[@]}"; do
        for THREAD in "${THREADS[@]}"; do
            for DATA_SIZE in "${DATA_SIZES[@]}"; do
                echo "  Configuration: Mode=$MODE, Threads=$THREAD, Data_Size=$DATA_SIZE"

                for RUN in $(seq 1 "$ITERATIONS"); do
                    run_perf_and_log "$PROGRAM" "$MODE" "$DATA_SIZE" "$THREAD" "$RUN"
                done
            done
        done
    done
done

echo "Performance data collection complete. Data saved to $OUTPUT_FILE."
