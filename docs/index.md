---
layout: home
title: LearnC++ Plus
description: 补齐 learncpp 没讲完的现代 C++：C++20/23/26 的特性、可编译示例与编译器支持矩阵。

hero:
  name: LearnC++ Plus
  text: 补齐 learncpp 没讲完的现代 C++
  tagline: C++20 / C++23 / C++26 特性、可编译示例、Pele 渲染的图
  actions:
    - theme: brand
      text: 从缺口地图开始
      link: /guide/why
    - theme: alt
      text: 去看 learncpp
      link: https://www.learncpp.com/

features:
  - title: 只讲缺口
    details: learncpp 没覆盖、或只开了个头的主题，从这里接着往下讲。
  - title: 示例可编译
    details: 每个示例对应一份 snippet，在本机 clang++ 上编译并比对输出。
  - title: 支持情况不糊弄
    details: 能验证的标绿；本机验证不了的标黄，并写明需要的编译器与标准库版本。
  - title: 图由 Pele 渲染
    details: 不用 mermaid.js；Pele 在客户端挂载，窄屏自动重排，可缩放拖拽。
---

## 关于本站

learncpp 是一条很好的主线，但它在第 18 章之后放慢了脚步：ranges、concepts、modules、协程、`<format>`、`<expected>` 这些现代 C++ 的核心内容，要么只有简介，要么还没写。

本站当作它的续篇：语言特性、标准库、并发与 C++26 跟踪，按 learncpp 的进度接着读即可。正文用中文，术语保留英文原文，方便你回去对照 cppreference。

示例不是伪代码。每段代码都有对应的 snippet 文件，由本机 `clang++` 编译、运行、比对输出；跑不通的例子不会出现在页面上。
