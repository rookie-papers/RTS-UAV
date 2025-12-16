import subprocess
import re
import sys
import os
import csv

# ================= CONFIGURATION =================
# Test values for N and TM (assuming N == TM)
TEST_VALUES = [4, 16, 64, 128]

# Compile Command
# -DCMAKE_BUILD_TYPE=Release: Ensure optimizations are enabled (O3)
# -DN=... -DTM=...: Pass macro definitions to C++ code
def get_build_cmd(n_val, tm_val):
    return f"cmake -DCMAKE_BUILD_TYPE=Release -DN={n_val} -DTM={tm_val} .. && make -j4"

# Run Command
# --benchmark_min_time=0.01s:
# Forces Google Benchmark to collect data for at least 0.01 seconds per test.
# This prevents "too fast" functions from triggering excessive iterations.
RUN_CMD = "./benchmark_RTS --benchmark_min_time=0.5s"

# Output Filename for Excel/CSV
CSV_FILENAME = "benchmark_results.csv"
# ===============================================

def run_command_realtime(command):
    """
    Executes a shell command, prints output in real-time to the console,
    and returns the full captured output for parsing.
    """
    process = subprocess.Popen(
        command,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, # Merge stderr into stdout
        text=True,
        bufsize=1 # Line buffered
    )

    full_output = []

    print(f"Executing: {command}")
    print("-" * 40)

    # Read output line by line in real-time
    while True:
        line = process.stdout.readline()
        if not line and process.poll() is not None:
            break
        if line:
            # Print to console (strip to avoid double newlines)
            print(line.strip())
            full_output.append(line)

    print("-" * 40)
    return "".join(full_output), process.returncode

def parse_output(output):
    """
    Parses Google Benchmark output using Regex to extract timings.
    Returns a dictionary, e.g., {'BM_Setup': '242050', ...}
    """
    results = {}
    # Regex: Matches "BM_Name" + whitespace + "Digits" + "ns"
    pattern = re.compile(r'(BM_\w+)\s+(\d+)\s+ns')
    for line in output.splitlines():
        match = pattern.search(line)
        if match:
            results[match.group(1)] = match.group(2)
    return results

def main():
    table_data = []
    # Define Table Headers
    headers = ["N / TM", "Setup(ns)", "KeyGen(ns)", "Sign(ns)", "Tran(ns)", "Verify(ns)", "Hash(ns)"]

    # Initialize CSV file with headers
    try:
        with open(CSV_FILENAME, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(headers)
    except IOError as e:
        print(f"Warning: Could not create file {CSV_FILENAME}. {e}")

    print(f"=== Starting Automation Test: {len(TEST_VALUES)} Sets ===")

    for val in TEST_VALUES:
        n = val
        tm = val

        print(f"\n[ Testing N={n}, TM={tm} ]")

        # 1. Compilation
        build_cmd = get_build_cmd(n, tm)
        # Suppress build output (stdout=DEVNULL) unless error occurs
        ret = subprocess.run(build_cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

        if ret.returncode != 0:
            print(f"❌ Build Failed: N={n}")
            print(ret.stderr.decode())
            continue

        # 2. Execution (with real-time output)
        output, ret_code = run_command_realtime(RUN_CMD)

        if ret_code != 0:
            print(f"❌ Runtime Error (Code {ret_code})")
            continue

        # 3. Parsing Results
        parsed = parse_output(output)

        row_dict = {
            "N / TM": str(n),
            "Setup(ns)": parsed.get("BM_Setup", "N/A"),
            "KeyGen(ns)": parsed.get("BM_KeyGen", "N/A"),
            "Sign(ns)": parsed.get("BM_Sign", "N/A"),
            "Tran(ns)": parsed.get("BM_Tran", "N/A"),
            "Verify(ns)": parsed.get("BM_Verify", "N/A"),
            "Hash(ns)": parsed.get("BM_hash", "N/A")
        }
        table_data.append(row_dict)

        # 4. Write to CSV immediately (to save progress)
        line_items = [row_dict[h] for h in headers]
        with open(CSV_FILENAME, 'a', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(line_items)

    # 5. Print Final Markdown Table
    print("\n\n=== Final Test Results ===")

    # Format: First column 8 chars wide, others 15 chars wide
    row_fmt = "| {:<8} | {:<15} | {:<15} | {:<15} | {:<15} | {:<15} | {:<15} |"

    print("-" * 125)
    print(row_fmt.format(*headers)) # Print Headers
    print("|" + "-"*10 + "|" + ("-"*17 + "|") * 6) # Separator

    for row in table_data:
        items = [row[h] for h in headers]
        print(row_fmt.format(*items))

    print("-" * 125)
    print(f"✅ Results saved to: {os.path.abspath(CSV_FILENAME)}")

if __name__ == "__main__":
    main()