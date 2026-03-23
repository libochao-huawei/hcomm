#!/usr/bin/env bash
# 下载并安装 Google Gemini CLI（需要 Node.js 18+）
# 用法: bash scripts/install_gemini_cli.sh
# 若已有 Node 18+，仅安装 CLI: bash scripts/install_gemini_cli.sh --cli-only

set -e

GEMINI_CLI_NPM="@google/gemini-cli"
CLI_ONLY="${1:-}"

# 检测 Node 是否可用且版本 >= 18
node_ok() {
  command -v node >/dev/null 2>&1 && node -e "process.exit(process.versions.node.split('.')[0] >= 18 ? 0 : 1)" 2>/dev/null
}

# 若没有 Node 或版本过低，尝试用 nvm 安装（可选）
ensure_node() {
  if node_ok; then
    echo "已检测到 Node.js $(node -v)，跳过安装。"
    return 0
  fi

  if [ "$CLI_ONLY" = "--cli-only" ]; then
    echo "未检测到 Node.js 18+。请先安装 Node.js，例如："
    echo "  Ubuntu/WSL: sudo apt update && sudo apt install -y nodejs npm"
    echo "  或安装 nvm 后: nvm install 20 && nvm use 20"
    echo "然后重新运行本脚本，或执行: npm install -g $GEMINI_CLI_NPM"
    exit 1
  fi

  export NVM_DIR="${NVM_DIR:-$HOME/.nvm}"
  if [ -s "$NVM_DIR/nvm.sh" ]; then
    echo "正在使用已有 nvm..."
    . "$NVM_DIR/nvm.sh"
  else
    echo "正在安装 nvm 到 $NVM_DIR ..."
    mkdir -p "$NVM_DIR"
    curl -fsSL https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.1/install.sh | bash
    . "$NVM_DIR/nvm.sh"
  fi

  nvm install 20
  nvm use 20
}

# 安装 Gemini CLI
install_gemini() {
  if ! node_ok; then
    echo "错误: 需要 Node.js 18 或更高版本。请先安装 Node 或运行本脚本（将自动通过 nvm 安装）。" >&2
    exit 1
  fi

  echo "正在安装 Gemini CLI: $GEMINI_CLI_NPM ..."
  npm install -g "$GEMINI_CLI_NPM"
}

# 主流程
main() {
  ensure_node
  install_gemini
  echo ""
  echo "安装完成。运行以下命令使用 Gemini CLI："
  echo "  gemini"
  echo ""
  echo "首次运行会提示用 Google 账号登录；也可设置 API Key："
  echo "  export GEMINI_API_KEY=\"你的密钥\""
  echo ""
  if command -v gemini >/dev/null 2>&1; then
    echo "当前版本: $(gemini --version 2>/dev/null || true)"
  fi
}

main "$@"
