#!/bin/bash

# Clean code check script for C/C++ code
# Supports full codebase check and incremental check
# Supports whitelist functionality

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
CHECK_MODE="full"  # full or incremental
WHITELIST_FILE=".cleancode_whitelist.txt"
REPORT_DIR="cleancode_reports"
BUILD_DIR="build"

# Print usage
usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -m MODE, --mode MODE     Check mode: full (default) or incremental"
    echo "  -w FILE, --whitelist FILE Whitelist file path"
    echo "  -r DIR, --report DIR     Report directory"
    echo "  -b DIR, --build DIR      Build directory (for clang-tidy)"
    echo "  -h, --help               Show this help message"
    echo ""
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -m|--mode)
            CHECK_MODE="$2"
            shift # past argument
            shift # past value
            ;;
        -w|--whitelist)
            WHITELIST_FILE="$2"
            shift # past argument
            shift # past value
            ;;
        -r|--report)
            REPORT_DIR="$2"
            shift # past argument
            shift # past value
            ;;
        -b|--build)
            BUILD_DIR="$2"
            shift # past argument
            shift # past value
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            usage
            exit 1
            ;;
    esac

done

# Check if required tools are installed
check_tools() {
    echo -e "${YELLOW}Checking required tools...${NC}"
    
    if ! command -v cppcheck &> /dev/null; then
        echo -e "${RED}Error: cppcheck is not installed${NC}"
        echo -e "${YELLOW}Please install cppcheck: sudo apt install cppcheck${NC}"
        return 1
    fi
    
    if ! command -v clang-tidy &> /dev/null; then
        echo -e "${YELLOW}Warning: clang-tidy is not installed${NC}"
        echo -e "${YELLOW}Some checks will be skipped${NC}"
        CLANG_TIDY_AVAILABLE=false
    else
        CLANG_TIDY_AVAILABLE=true
    fi
    
    echo -e "${GREEN}Required tools check completed${NC}"
}

# Create report directory
create_report_dir() {
    mkdir -p "$REPORT_DIR"
    echo -e "${GREEN}Report directory created: $REPORT_DIR${NC}"
}

# Load whitelist
load_whitelist() {
    if [[ -f "$WHITELIST_FILE" ]]; then
        echo -e "${YELLOW}Loading whitelist from $WHITELIST_FILE...${NC}"
        # Use safer approach to read whitelist
        WHITELIST=""
        while IFS= read -r line; do
            # Skip comments and empty lines
            [[ $line =~ ^#.*$ ]] && continue
            [[ -z $line ]] && continue
            WHITELIST+=" $line"
        done < "$WHITELIST_FILE"
        echo -e "${GREEN}Whitelist loaded${NC}"
    else
        echo -e "${YELLOW}Whitelist file not found: $WHITELIST_FILE${NC}"
        echo -e "${YELLOW}Creating empty whitelist file${NC}"
        touch "$WHITELIST_FILE"
        WHITELIST=""
    fi
}

# Check if file is in whitelist
is_in_whitelist() {
    local file="$1"
    for pattern in $WHITELIST; do
        if [[ "$file" == *"$pattern"* ]]; then
            return 0
        fi
    done
    return 1
}

# Run cppcheck
run_cppcheck() {
    local files="$1"
    local report_file="$REPORT_DIR/cppcheck_report.txt"
    
    echo -e "${YELLOW}Running cppcheck...${NC}"
    
    # Filter out whitelisted files
    local filtered_files=""
    for file in $files; do
        if ! is_in_whitelist "$file"; then
            filtered_files+=" $file"
        fi
    done
    
    if [[ -z "$filtered_files" ]]; then
        echo -e "${GREEN}All files are whitelisted, skipping cppcheck${NC}"
        return 0
    fi
    
    cppcheck --enable=all --std=c++11 --verbose $filtered_files 2>&1 | tee "$report_file"
    
    # Check if there are errors
    if grep -q "error:" "$report_file"; then
        echo -e "${RED}cppcheck found errors!${NC}"
        return 1
    else
        echo -e "${GREEN}cppcheck completed successfully${NC}"
        return 0
    fi
}

# Run clang-tidy
run_clang_tidy() {
    if [[ "$CLANG_TIDY_AVAILABLE" == false ]]; then
        return 0
    fi
    
    local files="$1"
    local report_file="$REPORT_DIR/clang-tidy_report.txt"
    
    echo -e "${YELLOW}Running clang-tidy...${NC}"
    
    # Check if build directory exists
    if [[ ! -d "$BUILD_DIR" ]]; then
        echo -e "${YELLOW}Build directory not found, skipping clang-tidy${NC}"
        return 0
    fi
    
    # Filter out whitelisted files
    local filtered_files=""
    for file in $files; do
        if ! is_in_whitelist "$file"; then
            filtered_files+=" $file"
        fi
    done
    
    if [[ -z "$filtered_files" ]]; then
        echo -e "${GREEN}All files are whitelisted, skipping clang-tidy${NC}"
        return 0
    fi
    
    clang-tidy -p "$BUILD_DIR" $filtered_files 2>&1 | tee "$report_file"
    
    # Check if there are errors
    if grep -q "error:" "$report_file"; then
        echo -e "${RED}clang-tidy found errors!${NC}"
        return 1
    else
        echo -e "${GREEN}clang-tidy completed successfully${NC}"
        return 0
    fi
}

# Get all C/C++ files
get_all_files() {
    find src -type f -name "*.cpp" -o -name "*.c" | tr '\n' ' '
}

# Get incremental files (changed files)
get_incremental_files() {
    git diff --name-only --diff-filter=ACM | grep -E "\.(cpp|c)$" | tr '\n' ' '
}

# Main function
main() {
    echo -e "${GREEN}=======================================${NC}"
    echo -e "${GREEN}C/C++ Clean Code Checker${NC}"
    echo -e "${GREEN}=======================================${NC}"
    
    # Check tools
    check_tools || exit 1
    
    # Create report directory
    create_report_dir
    
    # Load whitelist
    load_whitelist
    
    # Get files to check
    if [[ "$CHECK_MODE" == "full" ]]; then
        echo -e "${YELLOW}Running full codebase check...${NC}"
        FILES=$(get_all_files)
    else
        echo -e "${YELLOW}Running incremental check...${NC}"
        FILES=$(get_incremental_files)
        
        if [[ -z "$FILES" ]]; then
            echo -e "${GREEN}No changed files found, skipping check${NC}"
            exit 0
        fi
    fi
    
    echo -e "${YELLOW}Files to check:${NC}"
    echo "$FILES"
    echo
    
    # Run checks
    local cppcheck_result=0
    local clang_tidy_result=0
    
    run_cppcheck "$FILES" || cppcheck_result=1
    echo
    run_clang_tidy "$FILES" || clang_tidy_result=1
    echo
    
    # Summary
    echo -e "${GREEN}=======================================${NC}"
    echo -e "${GREEN}Check Summary${NC}"
    echo -e "${GREEN}=======================================${NC}"
    echo -e "Mode: $CHECK_MODE"
    echo -e "Files checked: $(echo $FILES | wc -w)"
    echo -e "Report directory: $REPORT_DIR"
    
    if [[ $cppcheck_result -eq 0 && $clang_tidy_result -eq 0 ]]; then
        echo -e "${GREEN}All checks passed!${NC}"
        return 0
    else
        echo -e "${RED}Some checks failed!${NC}"
        echo -e "${YELLOW}Please review the reports in $REPORT_DIR${NC}"
        return 1
    fi
}

# Run main
main

exit $?
