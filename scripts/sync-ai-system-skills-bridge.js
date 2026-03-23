#!/usr/bin/env node
const fs = require('fs');
const path = require('path');

const repoRoot = path.resolve(__dirname, '..');
const bridgeRoot = path.join(repoRoot, '.agents', 'skills');
const aiSystemRoot = path.join(repoRoot, '.ai-system');
const manifestPath = path.join(bridgeRoot, '.managed-by-ai-system.json');

function ensureDir(dir) {
  fs.mkdirSync(dir, { recursive: true });
}

function listExportableSkills(kind) {
  const root = path.join(aiSystemRoot, 'skills', kind);
  if (!fs.existsSync(root)) return [];

  return fs.readdirSync(root, { withFileTypes: true })
    .filter((entry) => entry.isDirectory())
    .map((entry) => ({
      name: entry.name,
      kind,
      sourceDir: path.join(root, entry.name)
    }))
    .filter((entry) => fs.existsSync(path.join(entry.sourceDir, 'SKILL.md')));
}

function writeManifest(managedSkills) {
  const payload = {
    generatedAt: new Date().toISOString(),
    sourceRoot: '.ai-system',
    policy: 'repo-local bridge only; do not mirror these skills into ~/.codex/skills',
    managedSkills
  };

  fs.writeFileSync(manifestPath, `${JSON.stringify(payload, null, 2)}\n`, 'utf8');
}

function main() {
  ensureDir(bridgeRoot);

  const skills = [
    ...listExportableSkills('internal'),
    ...listExportableSkills('external')
  ].sort((a, b) => a.name.localeCompare(b.name));

  const managedSkills = [];
  for (const skill of skills) {
    const linkPath = path.join(bridgeRoot, skill.name);
    const relativeSource = path.relative(path.dirname(linkPath), skill.sourceDir);
    if (!fs.existsSync(linkPath)) {
      fs.symlinkSync(relativeSource, linkPath, 'dir');
    }

    managedSkills.push({
      name: skill.name,
      kind: skill.kind,
      source: path.relative(repoRoot, skill.sourceDir).replace(/\\/g, '/'),
      linkMode: 'symlink'
    });
  }

  writeManifest(managedSkills);

  console.log(JSON.stringify({
    success: true,
    bridgeRoot: '.agents/skills',
    exported: managedSkills.length,
    policy: 'repo-local bridge only'
  }, null, 2));
}

main();
