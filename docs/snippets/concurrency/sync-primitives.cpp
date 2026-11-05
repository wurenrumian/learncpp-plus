// std: c++20
// expect:
// latch     : try_wait before start = false
// init worker 1: done
// init worker 2: done
// init worker 3: done
// latch     : all ready, 3 workers checked in
// barrier   : phase 1 complete
// barrier   : phase 2 complete
// barrier   : both writes visible = 30 30
// semaphore : peak concurrency = 2 of limit 2
// semaphore : 4 tasks done
// /expect
#include <atomic>
#include <barrier>
#include <cstdio>
#include <latch>
#include <semaphore>
#include <thread>
#include <vector>

int main() {
    // ── 1. latch：主线程等 3 个 worker 初始化完成 ──────────────────
    constexpr int kWorkers = 3;
    std::latch ready{kWorkers + 1};  // 3 个 worker + 主线程自己那一下
    std::printf("latch     : try_wait before start = %s\n",
                ready.try_wait() ? "true" : "false");

    // 打印顺序必须确定，所以用一串 binary_semaphore 传"打印令牌"。
    // 初始化工作本身是并行的，被串起来的只有 printf 这一行。
    std::binary_semaphore baton[kWorkers] = {
        std::binary_semaphore{0}, std::binary_semaphore{0}, std::binary_semaphore{0}};

    std::vector<std::thread> pool;
    for (int i = 0; i < kWorkers; ++i) {
        pool.emplace_back([&, i] {
            baton[i].acquire();  // 拿到自己那一棒
            // 模拟初始化：真实代码里可能是加载配置、打开连接
            std::printf("init worker %d: done\n", i + 1);
            if (i + 1 < kWorkers) {
                baton[i + 1].release();  // release 不要求所有权：谁都能放行
            }
            ready.count_down();  // 只减一，不阻塞
        });
    }

    baton[0].release();       // 主线程发第一棒
    ready.arrive_and_wait();  // 计数到零（3 个 worker + 自己）
    std::printf("latch     : all ready, %d workers checked in\n", kWorkers);

    for (auto& t : pool) {
        t.join();
    }

    // ── 2. barrier：两个 worker 分两阶段推进 ──────────────────────
    constexpr int kRacers = 2;
    std::atomic<int> phase_no{0};
    int phase1[kRacers] = {0, 0};
    int phase2[kRacers] = {0, 0};

    std::barrier sync_point{kRacers, [&phase_no]() noexcept {
                               // completion function：最后一个到达的线程执行，
                               // 此时其他线程还卡在屏障里，可以安全地准备下一阶段
                               int n = phase_no.fetch_add(1) + 1;
                               std::printf("barrier   : phase %d complete\n", n);
                           }};

    pool.clear();
    for (int i = 0; i < kRacers; ++i) {
        pool.emplace_back([&, i] {
            phase1[i] = (i + 1) * 10;           // 第一阶段：各写各的槽位
            sync_point.arrive_and_wait();       // 两阶段之间的分界
            phase2[i] = phase1[0] + phase1[1];  // 读得到对方的写入，说明屏障同步成功
            sync_point.arrive_and_wait();       // 第二阶段结束
        });
    }
    for (auto& t : pool) {
        t.join();
    }
    std::printf("barrier   : both writes visible = %d %d\n", phase2[0], phase2[1]);

    // ── 3. counting_semaphore<2>：4 个任务，最多 2 个同时跑 ────────
    constexpr int kTasks = 4;
    std::counting_semaphore<2> slots{2};  // 模板参数是上限，构造参数是初始值
    std::atomic<int> inside{0};
    std::atomic<int> peak{0};
    std::atomic<int> entered{0};
    std::latch gate{2};  // 最早进入的两个任务在这里会合，峰值 2 是撞出来的

    pool.clear();
    for (int i = 0; i < kTasks; ++i) {
        pool.emplace_back([&] {
            slots.acquire();  // 名额用完就在这里排队
            int now = inside.fetch_add(1) + 1;
            int seen = peak.load();
            while (now > seen && !peak.compare_exchange_weak(seen, now)) {
            }
            if (entered.fetch_add(1) < 2) {
                gate.arrive_and_wait();  // 只有最早的两个减计数，其余直接通过
            }
            inside.fetch_sub(1);
            slots.release();  // 归还名额，唤醒一个排队者
        });
    }
    for (auto& t : pool) {
        t.join();
    }
    std::printf("semaphore : peak concurrency = %d of limit 2\n", peak.load());
    std::printf("semaphore : %d tasks done\n", kTasks);
}
