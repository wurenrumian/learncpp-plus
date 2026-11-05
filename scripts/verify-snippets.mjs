#!/usr/bin/env node
/**
 * 编译并运行 docs/snippets/**\/*.cpp，把结果写进 docs/public/verify-report.json。
 *
 * snippet 头部协议：
 *   // std: c++23
 *   // expect:
 *   // line 1
 *   // /expect
 *
 * 约定：
 *   - expect 块内每行以 `// ` 开头，去掉前缀后与 stdout 逐行比较
 *   - stdout 只允许 ASCII（避免 Windows 代码页干扰）
 *   - 没有 expect 时只编译；`// skip-run` 强制只编译
 */
import { spawnSync } from 'node:child_process'
import { mkdir, readdir, readFile, writeFile } from 'node:fs/promises'
import { dirname, join, relative, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const snippetsDir = join(root, 'docs', 'snippets')
const reportPath = join(root, 'docs', 'public', 'verify-report.json')
const cxx = process.env.CXX || 'clang++'
const runId = new Date().toISOString().replace(/[:.]/g, '-')
const outDir = join(root, 'tmp', 'outputs', 'verify', runId)

function normalize(text) {
  return String(text ?? '')
    .replace(/\r\n?/g, '\n')
    .replace(/[ \t]+$/gm, '')
    .replace(/\n+$/g, '')
}

function parseSnippet(text) {
  const lines = text.split(/\r?\n/)
  let std = 'c++23'
  let expect = null
  let skipRun = false

  for (let i = 0; i < lines.length; i += 1) {
    const line = lines[i]

    const stdMatch = line.match(/^\/\/\s*std:\s*(\S+)/)
    if (stdMatch) {
      std = stdMatch[1]
      continue
    }

    if (/^\/\/\s*skip-run\s*$/.test(line)) {
      skipRun = true
      continue
    }

    if (/^\/\/\s*expect:\s*$/.test(line)) {
      const output = []
      for (let j = i + 1; j < lines.length; j += 1) {
        if (/^\/\/\s*\/expect\s*$/.test(lines[j])) break
        const content = lines[j].match(/^\/\/ ?(.*)$/)
        if (!content) break
        output.push(content[1])
      }
      expect = output.join('\n')
      break
    }
  }

  return { std, expect, skipRun }
}

async function findSnippets() {
  const entries = await readdir(snippetsDir, { recursive: true, withFileTypes: true })
  return entries
    .filter((entry) => entry.isFile() && entry.name.endsWith('.cpp'))
    .map((entry) => join(entry.parentPath ?? entry.path, entry.name))
    .sort()
}

function run(command, args, options = {}) {
  return spawnSync(command, args, {
    encoding: 'utf8',
    timeout: 30_000,
    windowsHide: true,
    ...options,
  })
}

const version = run(cxx, ['--version'])
if (version.error || version.status !== 0) {
  console.error(`无法执行 ${cxx}: ${version.error?.message ?? version.stderr}`)
  process.exit(1)
}
const clangVersion = (version.stdout || '').split('\n')[0].trim()

const files = await findSnippets()
if (files.length === 0) {
  console.error(`没有找到任何 snippet：${snippetsDir}`)
  process.exit(1)
}

await mkdir(outDir, { recursive: true })

const results = []
let failed = 0

for (const file of files) {
  const rel = relative(snippetsDir, file).replace(/\\/g, '/')
  const source = await readFile(file, 'utf8')
  const { std, expect, skipRun } = parseSnippet(source)
  const exe = join(outDir, rel.replace(/\.cpp$/, '.exe').replace(/\//g, '__'))
  const startedAt = Date.now()

  const compile = run(cxx, [
    `-std=${std}`,
    '-Wall',
    '-Wextra',
    '-O0',
    '-fno-diagnostics-color',
    file,
    '-o',
    exe,
  ])

  const entry = {
    file: rel,
    std,
    status: 'passed',
    compileExitCode: compile.status,
    runExitCode: null,
    durationMs: 0,
  }

  if (compile.error || compile.status !== 0) {
    entry.status = 'failed'
    entry.stage = 'compile'
    entry.message = normalize(compile.stderr || compile.error?.message || '').slice(0, 4000)
    results.push(entry)
    failed += 1
    continue
  }

  if (!skipRun && expect !== null) {
    const execution = run(exe, [])
    entry.runExitCode = execution.status
    const actual = normalize(execution.stdout)
    const wanted = normalize(expect)

    if (execution.error || execution.status !== 0) {
      entry.status = 'failed'
      entry.stage = 'run'
      entry.message = normalize(execution.stderr || execution.error?.message || '').slice(0, 4000)
      failed += 1
    } else if (actual !== wanted) {
      entry.status = 'failed'
      entry.stage = 'compare'
      entry.expected = wanted
      entry.actual = actual
      failed += 1
    }
  }

  entry.durationMs = Date.now() - startedAt
  results.push(entry)
}

const report = {
  generatedAt: new Date().toISOString(),
  toolchain: {
    cxx,
    version: clangVersion,
  },
  summary: {
    total: results.length,
    passed: results.length - failed,
    failed,
  },
  snippets: results,
}

await mkdir(dirname(reportPath), { recursive: true })
await writeFile(reportPath, `${JSON.stringify(report, null, 2)}\n`, 'utf8')

for (const entry of results) {
  if (entry.status === 'passed') {
    console.log(`PASS  ${entry.file}  (${entry.std}, ${entry.durationMs}ms)`)
  } else {
    console.error(`FAIL  ${entry.file}  [${entry.stage}]`)
    if (entry.message) console.error(entry.message)
    if (entry.stage === 'compare') {
      console.error(`--- expected ---\n${entry.expected}\n--- actual ---\n${entry.actual}`)
    }
  }
}

console.log(
  `\n${report.summary.passed}/${report.summary.total} passed; report: ${relative(root, reportPath)}`,
)

process.exit(failed === 0 ? 0 : 1)
