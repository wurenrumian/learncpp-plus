# LearnC++ Plus

补齐 [learncpp.com](https://www.learncpp.com/) 没有覆盖的现代 C++ 特性：C++20 / C++23 / C++26 的语言与标准库、并发与协程、工程化与编译器支持。

## 快速开始

```powershell
npm install
npm run docs:dev      # 本地预览
npm run verify        # 编译并运行 docs/snippets 下的示例
npm run docs:build    # 生产构建
npm run docs:preview  # 预览构建结果
```

要求 Node 20+。本机验证使用 `clang++`（当前实测 23.1.4 + libc++ 23）。

## 目录

```text
docs/
├─ .vitepress/
│  ├─ config.mts
│  ├─ plugins/pele.ts        # mermaid fence → parse 校验 → <PeleDiagram>
│  └─ theme/
│     ├─ PeleDiagram.vue     # 客户端 mount（IntersectionObserver + pele/lazy）
│     ├─ pele.css            # VitePress 变量 → --pele-*
│     └─ index.ts
├─ snippets/                 # 13 个可编译示例，按章节分组
├─ guide/  cpp20/  cpp23/  stdlib/  concurrency/  reference/
└─ public/                   # verify-report.json（生成物）
scripts/
└─ verify-snippets.mjs
```

已经写好的页面（17 篇，侧边栏按主题分组）：

- 开始：缺口地图、工具链与标准选择
- C++20：ranges、concepts、协程与 task、consteval/constinit、三路比较
- C++23：`std::expected`、deducing this、`std::format`/`std::print`、optional 单子操作
- 标准库：span/mdspan、flat_map/flat_set
- 并发：jthread/stop_token、latch/barrier/semaphore
- 参考：编译器支持矩阵

## Mermaid 图

用普通的 ` ```mermaid ` 围栏即可，**不会**经过 mermaid.js：

- 构建时用 `pele.parse()` 校验语法，错误会带 `文件:行:列` 直接让构建失败
- 页面输出 `<PeleDiagram>`，客户端进入视口后用 `pele/lazy` 的 `mountAsync()` 渲染
- 主题通过 CSS 变量跟随 VitePress 明暗模式，不需要重新渲染
- 只展示源码不渲染：用 ` ```mermaid-code `

## snippet 协议

```cpp
// std: c++23
// expect:
// 第一行输出
// 第二行输出
// /expect
```

`npm run verify` 会编译、运行并逐行比对 stdout，结果写入 `docs/public/verify-report.json`。输出限定 ASCII，避免 Windows 代码页干扰。

## 实测边界

clang 23.1.4 + libc++ 23 下：

- 可用：`<print> <format> <expected> <optional>`（单子操作）`<mdspan> <flat_map> <ranges> <span> <latch> <barrier> <semaphore> <thread>`（jthread）
- 缺头文件：`<generator> <hive> <inplace_vector> <simd> <stacktrace>`
- 缺编译器实现：reflection、contracts

具体逐项状态见 `docs/reference/compiler-support.md`。
