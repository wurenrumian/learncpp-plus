import { Buffer } from 'node:buffer'
import { readFileSync } from 'node:fs'
import { resolve } from 'node:path'
import type MarkdownIt from 'markdown-it'
import { parse, PeleError } from 'pele'

interface DiagramMeta {
  caption?: string
}

function parseMeta(raw: string): DiagramMeta {
  const meta: DiagramMeta = {}
  const re = /(caption|title)\s*=\s*(?:"([^"]*)"|'([^']*)')/g
  let match: RegExpExecArray | null
  while ((match = re.exec(raw)) !== null) {
    meta.caption = match[2] ?? match[3] ?? ''
  }
  return meta
}

function escapeAttr(value: string): string {
  return value
    .replace(/&/g, '&amp;')
    .replace(/"/g, '&quot;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
}

/**
 * VitePress 会把 frontmatter 从交给 markdown-it 的内容里剥掉，
 * 因此 token.map 的行号是「去掉 frontmatter 之后」的。报错时补回偏移，
 * 让读者看到的行号和编辑器里一致。
 */
function frontmatterOffset(env: { path?: string; relativePath?: string } | undefined): number {
  const relativePath = env?.relativePath
  const absolute = env?.path ?? (relativePath ? resolve(process.cwd(), 'docs', relativePath) : undefined)
  if (!absolute) return 0

  try {
    const lines = readFileSync(absolute, 'utf8').split(/\r?\n/)
    if (lines[0]?.replace(/^\uFEFF/, '').trim() !== '---') return 0
    for (let i = 1; i < lines.length; i += 1) {
      if (lines[i].trim() === '---') return i + 1
    }
  } catch {
    // 读不到文件就按 0 处理，至少不掩盖原始解析错误。
  }
  return 0
}

/**
 * 把 ```mermaid 围栏转换成 <PeleDiagram>。
 *
 * 构建时只做语法校验（pele.parse），不生成 SVG；真正的渲染在客户端
 * 由 PeleDiagram.vue 用 pele/lazy 的 mountAsync 完成。
 *
 * 支持的 info 写法：
 *   ```mermaid
 *   ```mermaid caption="ranges 管道"
 *   ```mermaid-code   // 只展示源码，不渲染
 */
export function pelePlugin(md: MarkdownIt): void {
  const defaultFence = md.renderer.rules.fence!

  md.renderer.rules.fence = (tokens, idx, options, env, self) => {
    const token = tokens[idx]
    const info = token.info.trim()

    if (!/^mermaid(?:-code)?(?:\s|$)/.test(info)) {
      return defaultFence(tokens, idx, options, env, self)
    }

    // mermaid-code：按普通 mermaid 代码块高亮展示，不渲染成图。
    if (/^mermaid-code(?:\s|$)/.test(info)) {
      const original = token.info
      token.info = 'mermaid'
      try {
        return defaultFence(tokens, idx, options, env, self)
      } finally {
        token.info = original
      }
    }

    const source = token.content.replace(/\r\n?/g, '\n').trim()
    const meta = parseMeta(info.slice('mermaid'.length))
    const mdEnv = env as { path?: string; relativePath?: string } | undefined
    const file = mdEnv?.relativePath ?? 'unknown.md'
    const fenceLine = (token.map?.[0] ?? 0) + 1

    try {
      parse(source)
    } catch (error) {
      if (error instanceof PeleError) {
        const line = frontmatterOffset(mdEnv) + fenceLine + (error.line > 0 ? error.line : 0)
        throw new Error(
          `Pele 无法解析 ${file}:${line}:${error.column || 1} — ${error.message}\n${error.snippet ?? ''}`,
        )
      }
      throw error
    }

    const encoded = Buffer.from(source, 'utf8').toString('base64')
    const caption = meta.caption ? ` caption="${escapeAttr(meta.caption)}"` : ''
    return `<PeleDiagram src="${encoded}"${caption} />\n`
  }
}
