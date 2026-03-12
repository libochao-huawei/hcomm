#!/usr/bin/env python3
import os
import re
from collections import defaultdict

def find_all_files(root_dir, exclude_dir):
    files = []
    exclude_path = os.path.join(root_dir, exclude_dir)
    for root, dirs, filenames in os.walk(root_dir):
        if exclude_path in root:
            continue
        for filename in filenames:
            if filename.endswith(('.h', '.cc', '.cpp')):
                files.append(os.path.join(root, filename))
    return files

def find_all_files_including_hccp(root_dir):
    files = []
    for root, dirs, filenames in os.walk(root_dir):
        for filename in filenames:
            if filename.endswith(('.h', '.cc', '.cpp')):
                files.append(os.path.join(root, filename))
    return files

def extract_structs(filepath):
    structs = set()
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        struct_pattern = r'struct\s+(\w+)\s*\{'
        for match in re.finditer(struct_pattern, content):
            structs.add(match.group(1))
    except Exception as e:
        pass
    return structs

def is_in_struct_or_class(content, pos):
    last_struct = content.rfind('struct', 0, pos)
    last_class = content.rfind('class', 0, pos)
    last_open_brace = content.rfind('{', 0, pos)
    
    if last_open_brace == -1:
        return False
    
    brace_count = 1
    i = last_open_brace + 1
    while i < len(content) and brace_count > 0:
        if content[i] == '{':
            brace_count += 1
        elif content[i] == '}':
            brace_count -= 1
        i += 1
    
    closing_brace = i - 1
    
    if last_open_brace < pos < closing_brace:
        if last_struct > last_class:
            return last_struct < last_open_brace
        else:
            return last_class < last_open_brace
    
    return False

def is_in_function_params(content, pos):
    last_open_paren = content.rfind('(', 0, pos)
    if last_open_paren == -1:
        return False
    
    paren_count = 1
    i = last_open_paren + 1
    while i < len(content) and paren_count > 0:
        if content[i] == '(':
            paren_count += 1
        elif content[i] == ')':
            paren_count -= 1
        i += 1
    
    closing_paren = i - 1
    return last_open_paren < pos < closing_paren

def is_typedef(content, pos):
    line_start = content.rfind('\n', 0, pos) + 1
    line_end = content.find('\n', pos)
    if line_end == -1:
        line_end = len(content)
    line = content[line_start:line_end]
    return 'typedef' in line

def fix_uninitialized_variables(filepath, structs):
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        
        original_content = content
        modifications = []
        
        for struct_name in structs:
            pattern = rf'({struct_name})\s+(\w+)\s*;'
            for match in re.finditer(pattern, content):
                var_name = match.group(2)
                
                line_start = content.rfind('\n', 0, match.start()) + 1
                line_end = content.find('\n', match.start())
                if line_end == -1:
                    line_end = len(content)
                line = content[line_start:line_end]
                
                if '=' in line:
                    continue
                
                if 'extern' in line:
                    continue
                
                if is_in_function_params(content, match.start()):
                    continue
                
                if is_typedef(content, match.start()):
                    continue
                
                if is_in_struct_or_class(content, match.start()):
                    continue
                
                old_text = match.group(0)
                new_text = f"{struct_name} {var_name} = {{}};"
                
                line_num = content[:match.start()].count('\n') + 1
                modifications.append({
                    'line': line_num,
                    'struct': struct_name,
                    'variable': var_name,
                    'old': old_text,
                    'new': new_text
                })
                
                content = content[:match.start()] + new_text + content[match.end():]
        
        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return modifications
        return []
                    
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        return []

def main():
    root_dir = "/root/code/hcomm/src/platform"
    exclude_dir = "hccp"
    
    # Collect struct definitions from ALL files (including hccp)
    print(f"Scanning platform directory for struct definitions (including {exclude_dir})...")
    all_files = find_all_files_including_hccp(root_dir)
    print(f"Found {len(all_files)} source files\n")
    
    print("Collecting struct definitions...")
    all_structs = set()
    for filepath in all_files:
        structs = extract_structs(filepath)
        all_structs.update(structs)
    
    print(f"Found {len(all_structs)} struct definitions\n")
    
    # Fix only files NOT in hccp directory
    print(f"Fixing uninitialized variables (excluding {exclude_dir})...")
    files_to_fix = find_all_files(root_dir, exclude_dir)
    print(f"Found {len(files_to_fix)} files to fix\n")
    
    all_modifications = defaultdict(list)
    
    for filepath in files_to_fix:
        modifications = fix_uninitialized_variables(filepath, all_structs)
        if modifications:
            all_modifications[filepath] = modifications
    
    print("\n" + "="*80)
    print("FIXED UNINITIALIZED STRUCT VARIABLES")
    print("="*80 + "\n")
    
    if not all_modifications:
        print("No uninitialized struct variables found to fix!")
        return
    
    total_count = 0
    for filepath, modifications in sorted(all_modifications.items()):
        rel_path = os.path.relpath(filepath, root_dir)
        print(f"\nFile: {rel_path}")
        print("-" * 80)
        
        for item in sorted(modifications, key=lambda x: x['line']):
            print(f"  Line {item['line']}: {item['struct']} {item['variable']}")
            print(f"    Changed: {item['old']} -> {item['new']}")
            total_count += 1
    
    print("\n" + "="*80)
    print(f"Total: {total_count} variables fixed")
    print("="*80)

if __name__ == "__main__":
    main()
