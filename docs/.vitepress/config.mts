import { defineConfig } from 'vitepress'
import { pelePlugin } from './plugins/pele'

export default defineConfig({
  lang: 'zh-CN',
  title: 'LearnC++ Plus',
  description: '补齐 learncpp 未覆盖的现代 C++ 特性',
  cleanUrls: true,

  markdown: {
    lineNumbers: true,
    config(md) {
      md.use(pelePlugin)
    },
  },

  themeConfig: {
    nav: [
      { text: '首页', link: '/' },
      { text: '开始', link: '/guide/why' },
      { text: 'C++20', link: '/cpp20/ranges' },
      { text: 'C++23', link: '/cpp23/expected' },
      { text: '标准库', link: '/stdlib/span' },
      { text: '并发', link: '/concurrency/jthread' },
      { text: '参考', link: '/reference/compiler-support' },
      { text: 'learncpp.com', link: 'https://www.learncpp.com/' },
    ],

    sidebar: [
      {
        text: '开始',
        items: [
          { text: '首页', link: '/' },
          { text: '为什么需要 LearnC++ Plus', link: '/guide/why' },
          { text: '本机工具链与标准选择', link: '/guide/toolchain' },
        ],
      },
      {
        text: 'C++20',
        items: [
          { text: 'ranges 与 views', link: '/cpp20/ranges' },
          { text: 'concepts 与 requires', link: '/cpp20/concepts' },
          { text: 'consteval、constinit 与 if consteval', link: '/cpp20/consteval' },
          { text: '三路比较 <=>', link: '/cpp20/spaceship' },
          { text: '协程：手写一个惰性 task', link: '/cpp20/coroutines' },
        ],
      },
      {
        text: 'C++23',
        items: [
          { text: 'std::expected', link: '/cpp23/expected' },
          { text: 'optional 的单子操作', link: '/cpp23/optional-monadic' },
          { text: 'std::format 与 std::print', link: '/cpp23/print-format' },
          { text: 'deducing this', link: '/cpp23/deducing-this' },
        ],
      },
      {
        text: '标准库',
        items: [
          { text: 'span 与 mdspan', link: '/stdlib/span' },
          { text: 'flat_map 与 flat_set', link: '/stdlib/flat-map' },
        ],
      },
      {
        text: '并发',
        items: [
          { text: 'jthread 与 stop_token', link: '/concurrency/jthread' },
          { text: 'latch / barrier / semaphore', link: '/concurrency/sync-primitives' },
        ],
      },
      {
        text: '参考',
        items: [
          { text: '编译器支持矩阵', link: '/reference/compiler-support' },
        ],
      },
    ],

    search: {
      provider: 'local',
    },

    outline: {
      level: [2, 3],
      label: '本页目录',
    },

    docFooter: {
      prev: '上一篇',
      next: '下一篇',
    },
  },
})
