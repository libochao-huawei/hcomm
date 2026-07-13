# HCCL_VM UT Test Execution Script

## Overview

`run_ut.sh` is the unit test execution script for the HCCL_VM project, used to automate the compilation and execution of test cases.

## Three-Step Process

The script automatically completes the following steps during execution:

| Step | Description | Log Output |
|------|-------------|------------|
| Step 1 | CMake configuration + make compilation | `build.log` |
| Step 2 | Generate executable files | Lists all binaries with size/time |
| Step 3 | Execute test cases and display results | `run.log` + `summary.log` |
| Step 4 | Generate gcov/lcov coverage HTML report (requires `--cov`) | `coverage.log` |

## Usage

```bash
cd {HCCL_VM_PATH}/test

# Basic usage
./run_ut.sh                           # Full compilation + execute all tests
./run_ut.sh --cov                     # Full compilation + execution + generate gcov/lcov coverage HTML report
./run_ut.sh <directory>               # Compile + execute all tests in specified directory (recursive)
./run_ut.sh <binary_name>             # Compile + execute specified test binary
./run_ut.sh <test_file_name>          # Compile + execute the binary corresponding to the specified test file
./run_ut.sh <file> <test_case_name>   # Compile + execute a single test case in the specified file

# Other commands
./run_ut.sh -l, --list                # List all available tests
./run_ut.sh -h, --help                # Display help information
```

## Command Details

### 1. Full Execution

```bash
./run_ut.sh
```

- Executes all tests in the `test` directory.
- Complete three-step process: compilation → generate executables → run all tests.
- Suitable for full regression testing.

### 2. Coverage Report

```bash
./run_ut.sh --cov
```

- Full compilation + execute all tests + generate gcov/lcov coverage HTML report.
- Automatically enables the `--coverage` compilation flag, generating `.gcno`/`.gcda` files.
- Four-step process: compilation (with coverage instrumentation) → execution → collect coverage data → generate HTML report.
- Report output path: `$CODE_DIR/coverage_report/html/index.html`.
- Automatically filters system headers, third-party libraries, stub files, and other non-business code.

### 3. Directory Execution

```bash
./run_ut.sh plugin/checker
./run_ut.sh plugin/ccu_executor
./run_ut.sh store
```

- Recursively finds all `*_test.cc` files in the specified directory.
- Compiles and executes all tests in that directory.
- Suitable for module-level testing.

### 4. Binary Execution

```bash
./run_ut.sh test_checker
./run_ut.sh test_allgather_semantics_checker
```

- Compiles and executes the specified test binary.
- Binary names start with `test_`.

### 5. File Execution

```bash
./run_ut.sh checker_test.cc
./run_ut.sh allgather_semantics_checker_test.cc
```

- Automatically matches the corresponding binary based on the test file name.
- Compiles and executes.

### 6. Single Test Case Execution

```bash
./run_ut.sh checker_test.cc CheckerTest.GenAndCheckGraph_EmptyQueues
./run_ut.sh allgather_semantics_checker_test.cc AllgatherSemanticsCheckerTest.CheckBasic
```

- Executes a single test case in the specified test file.
- Test case name format: `TestSuiteName.TestCaseName`.

### 7. List Tests

```bash
./run_ut.sh -l
./run_ut.sh --list
```

- Lists all available test files and their status.
- Displays the number of test cases and corresponding binary names.

## Examples

```bash
# Example 1: Full test
./run_ut.sh

# Example 2: Full test + coverage report
./run_ut.sh --cov

# Example 3: Execute all tests in the plugin/checker directory
./run_ut.sh plugin/checker

# Example 4: Compile and execute test_checker
./run_ut.sh test_checker

# Example 5: Compile and execute the binary corresponding to checker_test.cc
./run_ut.sh checker_test.cc

# Example 6: Execute a single test case
./run_ut.sh checker_test.cc CheckerTest.GenAndCheckGraph_EmptyQueues

# Example 7: List all tests
./run_ut.sh -l
```

## Log Directory

Each execution generates log files under `ut_logs/<timestamp>/`:

```text
{HCCL_VM_PATH}/ut_logs/20260425_142048/
├── build.log      # Detailed compilation log (cmake + make output)
├── run.log        # Detailed execution log (full output of each test)
└── summary.log    # Summary log (execution result of each test)
```

### Log File Description

| File | Content |
|------|---------|
| `build.log` | CMake configuration output, make compilation output, list of generated executables |
| `run.log` | Full output of each test (including gtest details) |
| `summary.log` | Execution status, pass/fail count, and time summary for each test |

## Execution Results

After the script finishes, summary information is displayed:

```text
========================================
  Directory Test Result Summary: plugin/checker
========================================
  Executed:    16
  PASSED:  185
  FAILED:  8
  CRASHED: 0
  TIMEOUT: 0
```

### Status Description

| Status | Description |
|--------|-------------|
| `PASS` | Test passed |
| `FAIL` | Test failed (assertion failure) |
| `CRASH` | Test crashed (core dump) |
| `TIMEOUT` | Test timed out (default 60 seconds) |

## Directory Structure

```text
test/
├── run_ut.sh              # This script
├── cmd/                   # Command-related tests
│   ├── base/
│   ├── subcmds/
│   └── utils/
├── device_arm/            # Device-related tests
├── device_vir/
├── ipc/                   # IPC-related tests
│   └── shm/
├── log/                   # Log-related tests
├── plugin/                # Plugin-related tests
│   ├── ccu_executor/
│   │   ├── control_type/
│   │   ├── load_type/
│   │   ├── reduce_type/
│   │   └── trans_type/
│   └── checker/
│       └── framework/
│           ├── mem_conflict_check/
│           ├── semantics_check/
│           └── singletask_check/
├── proxy/                 # Proxy-related tests
│   ├── level1/
│   └── level2/
├── runnerdb/              # Database-related tests
├── store/                 # Storage-related tests
│   └── hccl_shm/
└── src_root/              # Source root directory tests
```

## Environment Requirements

**Before executing the script, you must modify the following environment variables in `run_ut.sh` to use the actual paths:**

```bash
# The following are example paths. Modify them according to your actual environment.
export HCOMM_CODE_HOME=/home/q30033976/checker/hcomm      # hcomm source code path
export HCCL_CODE_HOME=/home/q30033976/checker/hccl        # hccl source code path (required for AIV/AICPU mode)
source /home/q30033976/checker/Ascend/cann/set_env.sh     # CANN environment script path
```

**Example**: If your working directory is `/home/workspace`, modify the settings to:

```bash
export HCOMM_CODE_HOME=/home/workspace/hcomm
export HCCL_CODE_HOME=/home/workspace/hccl
source /home/workspace/Ascend/cann/set_env.sh
```

The script will automatically load these environment variables. Failure to modify them will result in compilation errors.

## Configuration Parameters

The script has the following built-in configuration (can be modified at the beginning of the script):

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `CMAKE_BUILD_TYPE` | `Debug` | CMake build type |
| `MAKE_JOBS` | `8` | Number of parallel make jobs |
| `LOG_DIR` | `$CODE_DIR/ut_logs` | Log output directory |

## Notes

1. **First execution**: The first execution will perform a complete CMake configuration, which takes a longer time.
2. **Compilation failure**: If compilation fails, check `build.log` for detailed error information.
3. **Test failure**: If tests fail, check `run.log` for the specific failing test cases.
4. **Log cleanup**: Log directories are named by timestamp. Clean up old logs periodically to save space.

## Frequently Asked Questions

### Q: How to compile without executing?

A: The current script integrates compilation and execution. To compile only, use cmake and make directly:

```bash
cd {HCCL_VM_PATH}/build
cmake .. && make -j8 test_checker
```

### Q: How to view detailed output of a specific test?

A: Check the `run.log` file, which contains the complete gtest output for each test.

### Q: What to do if a test times out?

A: The default timeout is 60 seconds. You can modify the `timeout_sec` parameter in the script.

### Q: How to add new tests?

A: Create a `*_test.cc` file in the corresponding directory and add the build target to the corresponding `CMakeLists.txt`.

## Viewing Coverage Reports

After generating the HTML coverage report on Linux, start an HTTP server to view it:

```bash
cd <coverage_report/html directory>
python3 -m http.server 8080
```

- Access via local browser: `http://localhost:8080`.
- For remote servers, use SSH port forwarding to access locally:

```bash
ssh -L 8080:localhost:8080 <user>@<server_ip>
# Then open http://localhost:8080 in your local browser
```

Press `^C` to stop the server.

## Version History

- v1.0 - Initial version, supports full/directory/file/test-case-level test execution.
- v2.0 - Optimized command-line parameters, supports automatic path derivation, three-step process logging.
- v2.1 - Added `--cov` parameter, supports gcov/lcov coverage HTML report generation.
