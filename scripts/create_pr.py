#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
import urllib.request
import urllib.error
import urllib.parse
import yaml
import re
from datetime import datetime

CONFIG_FILE = "config.yaml"
DEFAULT_OWNER = "CANN"
DEFAULT_REPO = "hcomm"
DEFAULT_TARGET = "master"
DEFAULT_COMMENT = "/compile"
GITCODE_API_BASE = "https://gitcode.com/api/v5"
PR_TEMPLATE_PATH = ".gitcode/PULL_REQUEST_TEMPLATE.zh-CN.md"


def run_git_command(cmd, capture_output=True):
    try:
        result = subprocess.run(cmd, capture_output=capture_output, text=True, check=True)
        return result.stdout.strip() if capture_output else ""
    except subprocess.CalledProcessError as e:
        return e.stdout.strip() if e.stdout else ""


def load_config():
    if os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE, 'r') as f:
            config = yaml.safe_load(f)
            return config.get('gitcode_token', '')
    return ''


def get_current_branch():
    try:
        result = subprocess.run(['git', 'branch', '--show-current'],
                               capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return None


def get_default_title():
    branch = get_current_branch()
    if branch:
        return f"{branch}"
    return ""


def get_pr_template():
    if os.path.exists(PR_TEMPLATE_PATH):
        with open(PR_TEMPLATE_PATH, 'r', encoding='utf-8') as f:
            return f.read()
    return ""


def get_changed_files(target_branch):
    cmd = ['git', 'diff', '--name-only', f'origin/{target_branch}...HEAD']
    return run_git_command(cmd)


def get_commit_history(limit=10):
    cmd = ['git', 'log', f'origin/{DEFAULT_TARGET}..HEAD', f'--oneline', f'-{limit}']
    return run_git_command(cmd)


def get_commit_details(limit=5):
    cmd = ['git', 'log', f'origin/{DEFAULT_TARGET}..HEAD', '--format=%s%n---%b%n---', f'-{limit}']
    output = run_git_command(cmd)
    commits = []
    if output:
        parts = output.split('---')
        for part in parts:
            part = part.strip()
            if part:
                lines = part.split('\n')
                commits.append({
                    'message': lines[0] if lines else '',
                    'body': '\n'.join(lines[1:]) if len(lines) > 1 else ''
                })
    return commits


def analyze_change_type(files):
    change_types = {
        'added': [],
        'modified': [],
        'deleted': [],
        'renamed': []
    }
    
    cmd = ['git', 'diff', '--name-status', f'origin/{DEFAULT_TARGET}...HEAD']
    output = run_git_command(cmd)
    
    if not output:
        return change_types
    
    for line in output.split('\n'):
        if not line:
            continue
        parts = line.split('\t')
        if len(parts) >= 2:
            status = parts[0].strip().upper()
            filepath = parts[1].strip()
            if status == 'A':
                change_types['added'].append(filepath)
            elif status == 'M':
                change_types['modified'].append(filepath)
            elif status == 'D':
                change_types['deleted'].append(filepath)
            elif status == 'R':
                change_types['renamed'].append(filepath)
            else:
                change_types['modified'].append(filepath)
    
    return change_types


def generate_pr_description(changed_files, commits, change_types):
    template = get_pr_template()
    if not template:
        template = """## 描述
<!--在这里详细描述你的改动，包括改动的原因和所采取的方法。-->

## 关联的Issue
<!-- 如果这个PR是为了解决特定的Issue，请在这里提供Issue链接。-->
<!-- 如果这个PR不涉及Issue，可填写"NA"。-->

## 测试
<!--描述进行了哪些测试来验证你的改动。-->

## 文档更新
<!--如果这个PR包含文档的更新，请在这里指出。-->
"""

    branch = get_current_branch()
    
    added = change_types.get('added', [])
    modified = change_types.get('modified', [])
    deleted = change_types.get('deleted', [])
    
    file_summary = []
    if added:
        file_summary.append(f"新增文件 ({len(added)}): {', '.join(added[:5])}" + (" ..." if len(added) > 5 else ""))
    if modified:
        file_summary.append(f"修改文件 ({len(modified)}): {', '.join(modified[:5])}" + (" ..." if len(modified) > 5 else ""))
    if deleted:
        file_summary.append(f"删除文件 ({len(deleted)}): {', '.join(deleted[:3])}" + (" ..." if len(deleted) > 3 else ""))

    commit_summary = ""
    if commits:
        commit_messages = [c.get('message', '') or c.get('title', '') for c in commits[:5] if c]
        if commit_messages:
            commit_summary = "\n".join([f"- {msg}" for msg in commit_messages])

    pr_body = template
    
    pr_body = pr_body.replace("<!--在这里详细描述你的改动，包括改动的原因和所采取的方法。-->",
                              f"**分支**: {branch}\n\n**变更文件**: {len(changed_files.split(chr(10))) if changed_files else 0} 个文件\n" + 
                              "\n".join(file_summary) + 
                              (f"\n\n**主要提交**:\n{commit_summary}" if commit_summary else ""))

    pr_body = pr_body.replace("<!-- 如果这个PR是为了解决特定的Issue，请在这里提供Issue链接。-->",
                              "NA")
    pr_body = pr_body.replace('<!-- 如果这个PR不涉及Issue，可填写"NA"。-->',
                              "")

    pr_body = re.sub(r'<!--.*?-->', '', pr_body, flags=re.DOTALL)
    
    return pr_body


def generate_pr_title(commits):
    if commits:
        first_msg = commits[0].get('message', '') or commits[0].get('title', '')
        if first_msg:
            return first_msg.split('\n')[0].strip()
    
    branch = get_current_branch()
    if branch:
        prefixes = ['feat:', 'fix:', 'docs:', 'style:', 'refactor:', 'test:', 'chore:']
        for prefix in prefixes:
            if branch.startswith(prefix):
                return branch
        return f"{branch}"
    return "Update"


def create_pr(owner, repo, target_branch, title, body, head_branch, token):
    url = f"{GITCODE_API_BASE}/repos/{owner}/{repo}/pulls"
    
    data = {
        "title": title,
        "body": body,
        "head": head_branch,
        "base": target_branch
    }
    
    json_data = json.dumps(data).encode('utf-8')
    
    req = urllib.request.Request(url, data=json_data, method='POST')
    req.add_header('Content-Type', 'application/json')
    req.add_header('Accept', 'application/json')
    
    if token:
        req.add_header('Authorization', f'token {token}')
    
    try:
        with urllib.request.urlopen(req, timeout=30) as response:
            result = json.loads(response.read().decode('utf-8'))
            return True, result
    except urllib.error.HTTPError as e:
        error_body = e.read().decode('utf-8') if e.fp else str(e)
        return False, f"HTTP {e.code}: {error_body}"
    except urllib.error.URLError as e:
        return False, f"URL Error: {e.reason}"
    except Exception as e:
        return False, f"Error: {str(e)}"


def post_comment(owner, repo, pr_number, comment, token):
    url = f"{GITCODE_API_BASE}/repos/{owner}/{repo}/pulls/{pr_number}/comments"
    
    data = {
        "body": comment
    }
    
    json_data = json.dumps(data).encode('utf-8')
    
    req = urllib.request.Request(url, data=json_data, method='POST')
    req.add_header('Content-Type', 'application/json')
    req.add_header('Accept', 'application/json')
    
    if token:
        req.add_header('Authorization', f'token {token}')
    
    try:
        with urllib.request.urlopen(req, timeout=30) as response:
            result = json.loads(response.read().decode('utf-8'))
            return True, result
    except urllib.error.HTTPError as e:
        error_body = e.read().decode('utf-8') if e.fp else str(e)
        return False, f"HTTP {e.code}: {error_body}"
    except Exception as e:
        return False, f"Error: {str(e)}"


def main():
    parser = argparse.ArgumentParser(description='Create GitCode Pull Request')
    parser.add_argument('--owner', default=DEFAULT_OWNER, help='Repository owner')
    parser.add_argument('--repo', default=DEFAULT_REPO, help='Repository name')
    parser.add_argument('--target', default=DEFAULT_TARGET, help='Target branch')
    parser.add_argument('--title', help='PR title')
    parser.add_argument('--body', default='', help='PR body (required unless --generate is used)')
    parser.add_argument('--comment', default=DEFAULT_COMMENT, help='Comment after PR creation')
    parser.add_argument('--no-comment', action='store_true', help='Disable auto comment')
    parser.add_argument('--generate', action='store_true', help='Auto generate PR title and body from commits and changes')
    parser.add_argument('--dry-run', action='store_true', help='Only generate PR content without creating PR')
    parser.add_argument('--show-diff', action='store_true', help='Show changed files summary')
    
    args = parser.parse_args()
    
    token = load_config()
    if not token and not args.dry_run:
        print("Warning: No gitcode_token found in config.yaml")
    
    branch = get_current_branch()
    if not branch:
        print("Error: Cannot get current git branch")
        sys.exit(1)
    
    changed_files = get_changed_files(args.target)
    commits = get_commit_details()
    change_types = analyze_change_type(changed_files)
    
    if args.show_diff:
        print("=== 变更文件 ===")
        print(changed_files if changed_files else "无变更")
        print("\n=== 提交历史 ===")
        print(get_commit_history())
        print("\n=== 变更统计 ===")
        print(f"新增: {len(change_types.get('added', []))} 个文件")
        print(f"修改: {len(change_types.get('modified', []))} 个文件")
        print(f"删除: {len(change_types.get('deleted', []))} 个文件")
        if not args.generate and not args.title and not args.dry_run:
            return
    
    if args.generate:
        title = args.title if args.title else generate_pr_title(commits)
        body = generate_pr_description(changed_files, commits, change_types)
    else:
        title = args.title if args.title else get_default_title()
        body = args.body
    
    print("\n" + "=" * 60)
    print("生成的 PR 内容:")
    print("=" * 60)
    print(f"\n标题: {title}\n")
    print("-" * 60)
    print("内容:")
    print(body)
    print("=" * 60 + "\n")
    
    if args.dry_run:
        print("[DRY-RUN] 仅生成内容，不创建 PR")
        return
    
    if not args.generate and args.body == parser.get_default('body'):
        print("Error: PR body is required. Use --generate to auto-generate or --body to specify manually")
        sys.exit(1)
    
    print(f"Creating PR:")
    print(f"  Owner: {args.owner}")
    print(f"  Repo: {args.repo}")
    print(f"  Title: {title}")
    print(f"  Head: {branch}")
    print(f"  Base: {args.target}")
    print()
    
    success, result = create_pr(
        owner=args.owner,
        repo=args.repo,
        target_branch=args.target,
        title=title,
        body=body,
        head_branch=branch,
        token=token
    )
    
    if success:
        result_dict = result if isinstance(result, dict) else {}
        pr_url = result_dict.get('html_url', '')
        pr_number = result_dict.get('iid', '')
        print(f"PR created successfully!")
        print(f"  URL: {pr_url}")
        print(f"  Number: {pr_number}")
        
        if not args.no_comment and args.comment:
            print(f"\nPosting comment: {args.comment}")
            comment_success, _ = post_comment(args.owner, args.repo, pr_number, args.comment, token)
            if comment_success:
                print("Comment posted successfully!")
            else:
                print("Failed to post comment, but PR was created.")
    else:
        print(f"Failed to create PR: {result}")
        sys.exit(1)


if __name__ == '__main__':
    main()
