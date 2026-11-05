---
title: latch、barrier 与 semaphore：C++20 的同步原语
description: std::latch 一次性倒计数、std::barrier 多阶段会合与 completion function、counting_semaphore 与 binary_semaphore 的用法，三者与 condition_variable 的选择表，以及计数归零、模板参数、跨线程 release、获取顺序等陷阱。
---

# latch、barrier 与 semaphore：C++20 的同步原语

`std::thread` 和 `std::mutex` 能让你把工作分出去，但"怎么知道所有线程都到齐了""怎么让一批线程一起进入下一阶段""怎么限制同时只有两个线程在跑"这三个问题，C++20 之前只能自己用 `mutex + condition_variable + 计数器` 拼。C++20 把这三件事各给了专门的原语：`std::latch`、`std::barrier`、`std::counting_semaphore`。本页讲清它们的语义、边界和用途。

## 动机：手写的"等 N 个线程就绪"太容易漏东西

先看一段很多人写过的代码。主线程要等 3 个 worker 完成初始化：

```cpp
#include <condition_variable>
#include <mutex>

std::mutex m;
std::condition_variable cv;
int done = 0;

void worker() {
    initialize();                 // 各干各的初始化
    {
        std::lock_guard<std::mutex> lock(m);
        ++done;
    }
    cv.notify_one();              // 忘了这句，主线程就永远等下去
}

void wait_all() {
    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [] { return done == 3; });   // 谓词得自己写对
}
```

一共四个必须同时对的地方：锁、计数器、`notify`、谓词。漏一个 `notify_one`，程序不报错，只是永远卡住；谓词写成 `done > 3`，同样是静默挂死。要"等 N 个"就必须配 N，写错量词没人提醒你。

多阶段更糟。让两个线程"每算完一步就等对方也算完，再一起进入下一步"，你需要在计数器之外再维护一个 `phase` 变量、决定用 `notify_one` 还是 `notify_all`、还得防"先到的线程已经把阶段推进了，后到的线程还在等上一个阶段"的竞态。样板代码长了，出错概率是乘法。

C++20 的三个原语就是把这三类同步点变成有名字、有明确前置条件的类型：

- `std::latch`：一次性的倒计数门闩，等 N 个事件发生。
- `std::barrier`：可复用的会合点，一批线程反复"到齐 → 一起走"。
- `std::counting_semaphore`：计数型许可，限制同时进行的数量。

## 最小示例

下面这个程序可以在本机直接编译运行，三段分别演示 latch、barrier 和 semaphore，输出完全确定（连续运行 5 次逐字节一致）：

<<< @/snippets/concurrency/sync-primitives.cpp

输出：

```text
latch     : try_wait before start = false
init worker 1: done
init worker 2: done
init worker 3: done
latch     : all ready, 3 workers checked in
barrier   : phase 1 complete
barrier   : phase 2 complete
barrier   : both writes visible = 30 30
semaphore : peak concurrency = 2 of limit 2
semaphore : 4 tasks done
```

三处值得注意的细节：

- latch 的计数是 `kWorkers + 1`：3 个 worker 各 `count_down()` 一次，主线程自己 `arrive_and_wait()` 再减一次。如果主线程只 `wait()` 不 `arrive`，计数应该写成 3。
- barrier 的 `phase 1 complete` 由 **completion function** 打印，而它运行在最后一个到达的线程上，不是在主线程上。因此这两行的顺序是确定的：所有 worker 都必须先到达。
- semaphore 那段没有靠 `sleep` 凑重叠：最早进入临界区的两个任务在 `gate` 这个 latch 上互相等了一下，所以"峰值并发 = 2"是撞出来的，不是打印出来的常量。

为了让输出确定，示例里用了一串 `binary_semaphore` 传递"打印令牌"（worker 1 → 2 → 3），并让主线程发起第一棒。初始化工作本身仍是并行的，被串起来的只有对 `printf` 的调用。真实项目里通常不需要这层包装——但如果你要写测试或教学示例，这是个可用的办法，而且顺带演示了"信号量可以由另一个线程 release"。

页内其余代码块都是说明性片段，只有上面这份文件经过 `npm run verify` 的编译与输出比对。

## 机制与语义

```mermaid
flowchart TB
    subgraph L["latch 一次性"]
        L1["主线程等 N 个任务就绪"] --> L2["count_down 只减不加"]
        L2 --> L3["归零后永久放行"]
    end
    subgraph B["barrier 多阶段"]
        B1["每个阶段重新计数"] --> B2["completion 在最后到达者执行"]
        B2 --> B3["参与数固定，可减员"]
    end
    subgraph S["semaphore 计数"]
        S1["acquire 借走一个名额"] --> S2["release 归还，可跨线程"]
        S2 --> S3["模板参数是上限"]
    end
```

三者的共同点是"等待一个计数条件"，区别在于**计数能不能涨回去**、**谁来执行收尾工作**、**等待的语义是不是绑定线程**。

### std::latch：一次性倒计数

```cpp
#include <latch>

std::latch gate{1};        // 初始计数
gate.count_down();         // 计数减 1，不阻塞
gate.arrive_and_wait();    // 计数减 1，然后阻塞到计数为 0
gate.wait();               // 只等，不减
bool now = gate.try_wait();// 不阻塞，立刻返回"是否已经归零"
```

要点：

- **只减不加**。`latch` 没有 `count_up`、没有 `reset()`，归零以后永久处于"已打开"状态，再 `wait()` 立刻返回。这正是"等一次就够"的语义。
- 计数一旦是 0，就不能再 `count_down()`。标准的前置条件要求计数大于 0，违反是未定义行为（见"常见坑"第一条——本页示例在起草时就踩到了：4 个任务对初始计数为 2 的 latch 调用 `arrive_and_wait`，计数被减成负数，程序静默挂死）。
- `arrive_and_wait()` 等价于 `count_down(1)` 加 `wait()`，是"我到了，并且我要等其他人"的合并写法。
- `try_wait()` 让你在"不想阻塞"的场合（比如 UI 线程做进度轮询）检查状态。
- 实现上是一个原子计数加等待队列，不需要锁，也不需要 `notify` 与 `wait` 手工配对，所以比手写 `condition_variable` 更省心，通常也更快。

典型用途：等待一组线程完成初始化、等待 N 个异步请求全部返回（配合回调里的 `count_down`）、给测试写"发令枪"。

### std::barrier：可复用的会合点

```cpp
#include <barrier>

std::barrier sync{2};               // 2 个参与者，没有 completion function
sync.arrive_and_wait();             // 到了，并等其他 1 个
```

`barrier` 每凑齐 `expected` 个 `arrive` 就"通过一轮"，然后自动重置为下一轮计数，所以可以放在循环里反复用：

```cpp
void worker(int id, std::barrier<>& phase) {
    for (int step = 0; step < 3; ++step) {
        compute_step(id, step);     // 本阶段的计算
        phase.arrive_and_wait();    // 所有人都算完，才一起进入下一阶段
    }
}
```

真正的价值在 **completion function**。它在最后一个线程到达时、在屏障被重置之前、在该线程上执行；此时其他参与者都还堵在 `wait` 里，不会碰共享数据。所以"准备下一阶段"的收尾代码放在这里最安全，不需要额外加锁：

```cpp
std::vector<int> front(1024), back(1024);

std::barrier phase{8, [&front, &back]() noexcept {
    front.swap(back);        // 双缓冲换页：所有人都在等，交换是安全的
}};
```

几个容易忽略的语义：

- **执行者是"最后一个到达的线程"**，不是主线程，也不是固定的某一个线程。所以 completion function 里不要操作线程局部状态，也不要假设自己的线程身份。
- 它在屏障重置之前运行，且此时其他线程被挡住——这就是它"可以安全准备下一阶段"的全部原因。
- 标准要求 completion function 是 `noexcept` 可调用的。抛异常直接 `std::terminate`，没有调用栈可以挽回。
- 它应该短。所有参与者都在等它，它慢多少，整个阶段就慢多少。
- `arrive_and_drop()`：本轮的 `arrive` 照算（不会让其他人白等），但之后永久少一个参与者。适合"某个 worker 干完自己的活提前退出"的流水线。**这个减员不可撤销**：barrier 没有 `arrive_and_add`，参与数不能涨回去。

### counting_semaphore 与 binary_semaphore：许可计数

```cpp
#include <chrono>
#include <semaphore>

std::counting_semaphore<2> slots{2};   // 模板参数是上限，构造参数是初始值
slots.acquire();                       // 有许可就拿走一个，没有就阻塞
slots.release();                       // 归还一个，可能唤醒等待者
bool got = slots.try_acquire();        // 不阻塞
bool got2 = slots.try_acquire_for(std::chrono::milliseconds{100});  // 最多等 100ms

std::binary_semaphore flag{0};         // 等价于 counting_semaphore<1>
```

几个关键点：

- **没有所有者概念**。`mutex` 记录"谁锁的"，只有锁的持有者能解锁；信号量只是一个计数器，谁都能 `release()`。所以它天然支持"一个线程等，另一个线程放行"这种事件通知：

  ```cpp
  std::binary_semaphore data_ready{0};

  void producer() { produce(); data_ready.release(); }
  void consumer() { data_ready.acquire(); consume(); }
  ```

- **不可重入**。同一个线程连续 `acquire()` 两次，第二次会把自己锁死：初始许可为 1 时，第一次拿到唯一那个许可，第二次永远等不到人 `release`。
- 适合"限制并发数""资源池计数""生产者/消费者队列的长度上限"。上面的最小示例就用 `counting_semaphore<2>` 把 4 个任务压到同时最多 2 个。
- `try_acquire_for` / `try_acquire_until` 返回 `bool`，超时是正常返回值而不是异常，过期了要自己决定降级还是重试。标准还允许它们**伪失败**（明明有许可却返回 `false`），所以判失败只能当作"这次没拿到"，不能当作"资源一定被占满"。

### 三者与 condition_variable 怎么选

| 你要解决的问题 | 该用什么 | 关键 API |
| --- | --- | --- |
| 等 N 个任务全部就绪，只等一次 | `std::latch` | `count_down`、`arrive_and_wait`、`wait`、`try_wait` |
| 多阶段流水线，每阶段全员会合 | `std::barrier` | `arrive_and_wait`、completion function、`arrive_and_drop` |
| 限制并发数、资源池借还 | `std::counting_semaphore<N>` | `acquire`、`release`、`try_acquire_for` |
| 单次事件通知（有无数据） | `std::binary_semaphore` | `acquire`、`release` |
| 保护共享数据，需要所有权与 RAII | `std::mutex` + `lock_guard` | `lock` / `unlock` |
| 等一个复杂条件、条件不只是一个计数 | `std::condition_variable` | `wait(lock, pred)` |

一句话记忆：**一次性等待用 latch，多阶段用 barrier，限制数量用 semaphore，保护数据仍然用 mutex**。这三个原语都不保护任何数据，它们只协调"时间"。

## 常见坑

### 1. latch 的计数只能减，归零后继续减是未定义行为

`latch` 用完就废：没有 `reset()`，也不能把计数加回去。想做"再来一轮"，就得换个对象，或者直接用 `barrier`。

更要小心的是**减多了**。本页示例在起草时就踩过这个坑：semaphore 那段本来是让 4 个任务都对一个初始计数为 2 的 latch 调 `arrive_and_wait()`，后两个任务在计数已经是 0 的时候又各减了一次。程序没有报错、没有断言，而是直接挂住，看上去和死锁一模一样，只能靠逐段加打印才定位到是哪一段。修正办法是只让最早进入的两个任务参与这个 latch：

```cpp
if (entered.fetch_add(1) < 2) {
    gate.arrive_and_wait();   // 只有前两个减计数
}
```

libc++ 对"计数已为 0 时再减"的处理是接着往下减并挂起，而不是崩溃，所以不要指望调试器给你一个干净的断言。

### 2. barrier 的 completion function 抛异常会 terminate

completion function 必须是 `noexcept` 可调用的。它一旦抛出，标准规定调用 `std::terminate`——不是 `std::unexpected`，不是"让参与者收到异常"，而是整个进程结束。它运行在"其他线程全都堵着"的时刻，本来也没有合理的传播路径。

所以里面只做不会失败的事：交换指针、改原子标志、`printf` 调试信息。要分配内存、要调可能抛异常的接口，就把工作挪到屏障之后的正常代码里。

### 3. barrier 的参与者数量固定，`arrive_and_drop` 之后不能反悔

`barrier` 的 `expected` 在构造时定死：不能扩容，不能"临时多来一个线程"。`arrive_and_drop()` 能让参与者**净减少**一个，但没有反向操作。

于是"线程池大小会变化"的场景不适合用 `barrier`：

- 想动态调整参与者，只能换一个新的 barrier，并确保旧的那个没人还在等；
- 参与者已经 `drop` 完、计数到 0 后，barrier 处于终止状态，后续 `arrive` 立即返回。

同理，`barrier` 的每一轮都要求"所有存活参与者"到齐。只要有一个人因为异常路径提前 `return` 而没 `arrive`，剩下的人就永久卡住——用 `arrive_and_drop` 的 RAII 包装来兜底，或者在启动时就规划好退出路径。

### 4. semaphore 的模板参数是最大值，不是初始值

这是最常见的误读：

```cpp
std::counting_semaphore<2> a{0};   // 上限 2，初始 0：前两个 acquire 全部阻塞
std::counting_semaphore<2> b{2};   // 上限 2，初始 2：可以同时进两个
```

`counting_semaphore<N>` 的 `N` 是 `LeastMaxValue`：标准只保证 `max() >= N`，实现可以支持得更大（本机 libc++ 的 `max()` 正好等于模板参数）。构造函数的参数才是初始许可数，写成 `counting_semaphore<2> s{2}` 还是 `counting_semaphore<2> s{0}` 完全是两种行为。

顺带一条：`release()` 把计数推过 `max()` 是未定义行为，所以它不能当"事件计数器"无限累加。

`binary_semaphore` 就是 `counting_semaphore<1>` 的别名。本机实测第二次 `release()` 之后计数确实变成了 2（读得到两次 `try_acquire` 成功），但那次 `release()` 已经违反前置条件，属于未定义行为——不要因为"看起来能用"就把它当计数器。

### 5. binary_semaphore 不是 mutex

长得像（一个许可 = 一把锁），语义差得远：

- **没有所有权**：任何线程都能 `release()`，包括根本没 `acquire()` 过的线程。这既是它的用处（跨线程放行），也是它的危险（"只有持有者能解锁"的假设不成立）。
- **不可重入**：同一线程 `acquire()` 两次会死锁，没有递归信号量。
- **没有 RAII 适配**：标准没给 `lock_guard` 的对应物，异常路径上要自己保证 `release()`。
- 它只传递"有一个许可"这个事实，不理解你保护的是哪块数据。

所以用它保护共享数据是可以编译的，但一旦有人写错 `release` 的位置，你没有任何机制能发现。保护数据请继续用 `std::mutex`；`binary_semaphore` 留给"通知某件事发生了"。

### 6. 多个信号量的获取顺序不一致会死锁

```cpp
// 线程 A
s1.acquire();
s2.acquire();
// 线程 B
s2.acquire();
s1.acquire();   // A 拿着 s1 等 s2，B 拿着 s2 等 s1
```

`std::scoped_lock` 能一次性锁多个 mutex 并内部避免死锁，但它**不接受信号量**。用信号量的场合只能自己定规矩：全局固定一个获取顺序（比如按资源 id 从小到大），或者用 `try_acquire_for` 带超时，拿不到就把已拿到的还回去。

### 7. `try_acquire_for` 返回 bool，超时得自己处理

```cpp
std::optional<Result> try_take() {
    if (!slots.try_acquire_for(std::chrono::milliseconds{100})) {
        // 超时不是异常：返回错误、降级、稍后重试，都要你决定
        return std::nullopt;
    }
    Result r = use_resource();
    slots.release();
    return r;
}
```

两个细节：一是超时后**没有**任何清理动作，也不会有异常，代码会继续往下跑；二是标准允许 `try_acquire*` **伪失败**，所以 `false` 只能理解为"这次没拿到"，不能用来判断"资源是不是被占满了"。要精确判断负载，得自己另外记计数。

上面这个写法的另一个毛病是：`use_resource()` 一旦抛异常，许可就再也还不回去了，计数会永久少一个。信号量没有 `lock_guard` 那样的标准包装，需要自己写个析构里 `release()` 的小守卫。

## 编译器支持

本机实测（clang 23.1.4，MSYS2 clang64，标准库 libc++ 230104）：

| 头文件 | feature-test macro | 本机取值 |
| --- | --- | --- |
| `<latch>` | `__cpp_lib_latch` | `201907` |
| `<barrier>` | `__cpp_lib_barrier` | `201907` |
| `<semaphore>` | `__cpp_lib_semaphore` | `201907` |

- `-std=c++20` 与 `-std=c++23` 各编译一次，`-Wall -Wextra` 下均 0 警告、退出码 0，运行输出与 `expect` 一致。
- `-std=c++17` 直接编译失败：`error: no type named 'latch' in namespace 'std'`，头文件内容被 `__cplusplus` 挡在 C++20 之后。这三个原语没有 C++17 版本，C++17 只能用本页开头的 `condition_variable` 手写。
- `std::barrier` 的 CTAD（`std::barrier sync{2, lambda}`）在本机可用，不必手写模板参数。
- GCC / MSVC 的版本门槛本机未验证，逐项对照见 [编译器支持矩阵](/reference/compiler-support)。

## 与 learncpp 的关系

learncpp 没有并发章节，本页的起点是大多数人手写的那套 `mutex + condition_variable + 计数器`。那套代码没有错，只是 C++20 之后，其中相当一部分样板可以直接换成标准类型：

- 手写"等所有线程完成初始化"的 `cv + 计数器`，换成 `std::latch` 之后，锁、谓词、`notify` 三处易错点一起消失。
- 多阶段流水线的 `phase` 变量 + `notify_all`，换成 `std::barrier`，阶段推进和"准备下一阶段"的收尾都有了固定位置（completion function）。
- 限制并发数时手搓的 `cv + 计数`，现在有 `std::counting_semaphore`。

还没有被这些原语覆盖的部分：线程的取消与协作停止（`std::jthread` 与 `stop_token`，见 [jthread 与 stop_token](/concurrency/jthread)）、把同步点藏进状态机的协程写法（见 [协程：手写一个惰性 task](/cpp20/coroutines)）。另外，这三个原语只协调时间，**不约束类型**——如果你想给"可等待的东西"写统一接口，那要用 concepts 收口，见 [concepts 与 requires](/cpp20/concepts)。

## 小练习

1. **用 latch 重写手写版本**：把本页开头那段 `mutex + condition_variable + done` 改成 `std::latch`，要求主线程不参与计数（用 `wait()` 而不是 `arrive_and_wait()`），并让 worker 数量变成一个运行期参数。
2. **给 barrier 加双缓冲**：写 8 个 worker 的 3 阶段计算，每个阶段结束时用 completion function 交换 `front` / `back` 两个 `std::vector`。验证：任何 worker 在阶段内读到的都是上一阶段的完整结果。
3. **用 semaphore 做限流器**：给一个"同时最多 3 个请求"的函数加上 `counting_semaphore<3>`，并额外记录"曾经排队过的最长时间"。提示：`acquire()` 阻塞前的时刻要自己打时间戳。
4. **故意制造死锁再修好**：两个 `binary_semaphore`，两个线程按相反顺序获取，先跑出死锁，再用固定顺序修好。然后解释为什么 `std::scoped_lock` 帮不上忙。
5. **验证 `arrive_and_drop`**：4 个参与者，其中 1 个在第二轮开始前 `arrive_and_drop()` 退出。确认剩下 3 个仍能正常推进，并解释如果你把 `drop` 放在 `arrive_and_wait` 之后会发生什么。
