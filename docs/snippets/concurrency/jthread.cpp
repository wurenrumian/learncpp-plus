// std: c++20
// expect:
// [1] poller: started, stop_possible=1 stop_requested=0
// [1] main: latch released, calling request_stop
// [1] poller: saw stop_requested, leaving the loop
// [1] main: joined
// [1] main: second request_stop returned 0, the flag is one-shot
// [2] waiter: started, it will sleep until a task arrives or stop is requested
// [2] main: no task queued, calling request_stop while holding the mutex
// [2] waiter: woke up, got_task=0 stop_requested=1
// [2] main: joined, wait(token, pred) returned false because the token fired
// [3] worker: started, waiting for stop
// [3] main: get_stop_token -> stop_possible=1 stop_requested=0
// [3] main: callback registered, calling request_stop
// [3] callback: runs inside request_stop, same_thread_as_main=1
// [3] main: request_stop returned 1, the callback line above was printed on this thread
// [3] main: joined, token.stop_requested=1
// [4] fresh stop_source: stop_possible=1 stop_requested=0
// [4] first request_stop returned 1, now stop_requested=1
// [4] second request_stop returned 0
// [4] default stop_token: stop_possible=0 stop_requested=0
// /expect
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <latch>
#include <mutex>
#include <stop_token>
#include <thread>

// [1] 轮询型 worker：线程函数自己查 token，不阻塞在等待原语上。
//     jthread 把 stop_token 作为第一个参数自动注入，不需要外面手动传。
void demo_polling() {
    std::latch started{1};

    std::jthread poller([&started](std::stop_token token) {
        std::printf("[1] poller: started, stop_possible=%d stop_requested=%d\n",
                    int(token.stop_possible()), int(token.stop_requested()));
        started.count_down();

        // 只靠 token 退出。这里的 sleep 只是别把 CPU 打满，不承担任何排序作用；
        // "started" 一定先于 main 的下一行，靠的是上面的 latch，不是时间。
        while (!token.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        std::printf("[1] poller: saw stop_requested, leaving the loop\n");
    });

    started.wait();
    std::printf("[1] main: latch released, calling request_stop\n");
    poller.request_stop();  // 置位 + 跑回调，然后就返回；不等待线程结束
    poller.join();          // 想等它真的走完，得自己 join（或等析构）

    std::printf("[1] main: joined\n");
    // request_stop 是一次性的：谁先请求谁拿到 true，之后永远是 false。
    const bool second = poller.request_stop();
    std::printf("[1] main: second request_stop returned %d, the flag is one-shot\n",
                int(second));
}

// [2] 阻塞型 worker：没有任务时它睡在条件变量上，
//     普通 wait 收不到"停止"信号，必须用 wait(lock, token, pred)。
void demo_cv_wait() {
    std::mutex mtx;
    std::condition_variable_any cv;
    bool have_task = false;
    std::latch ready{1};

    std::jthread waiter([&](std::stop_token token) {
        std::printf("[2] waiter: started, it will sleep until a task arrives or stop is requested\n");
        std::unique_lock<std::mutex> lock(mtx);
        ready.count_down();

        // wait 期间锁被释放；pred 为假时挂起；token 被请求停止时也能醒来并返回 false。
        const bool got_task = cv.wait(lock, token, [&have_task] { return have_task; });
        std::printf("[2] waiter: woke up, got_task=%d stop_requested=%d\n",
                    int(got_task), int(token.stop_requested()));
    });

    ready.wait();
    {
        // 持锁调用 request_stop，避免 worker 正卡在"pred 已检查、还没挂起"的窗口。
        std::lock_guard<std::mutex> lock(mtx);
        std::printf("[2] main: no task queued, calling request_stop while holding the mutex\n");
        waiter.request_stop();
    }
    waiter.join();
    std::printf("[2] main: joined, wait(token, pred) returned false because the token fired\n");
}

// [3] stop_callback：注册时就把回调挂在 token 上；
//     request_stop 在**调用者**线程上同步执行它，所以回调里的打印夹在 main 的两行之间。
void demo_callback() {
    const std::thread::id main_id = std::this_thread::get_id();
    std::latch started{1};

    std::jthread owner([&started](std::stop_token token) {
        std::printf("[3] worker: started, waiting for stop\n");
        started.count_down();
        while (!token.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // 故意不在停止后打印：此刻 main 可能还在 request_stop 里，
        // 那样两边抢 stdout，顺序就不确定了。
    });

    started.wait();
    const std::stop_token token = owner.get_stop_token();
    std::printf("[3] main: get_stop_token -> stop_possible=%d stop_requested=%d\n",
                int(token.stop_possible()), int(token.stop_requested()));

    std::stop_callback cb(token, [main_id] {
        std::printf("[3] callback: runs inside request_stop, same_thread_as_main=%d\n",
                    int(std::this_thread::get_id() == main_id));
    });

    std::printf("[3] main: callback registered, calling request_stop\n");
    const bool stopped = owner.request_stop();
    std::printf("[3] main: request_stop returned %d, the callback line above was printed on this thread\n",
                int(stopped));

    owner.join();
    std::printf("[3] main: joined, token.stop_requested=%d\n",
                int(token.stop_requested()));
}

// [4] 三件套本身：stop_source 是"开关"，stop_token 是"只读观察端"。
//     没有线程也能看清 stop_possible 与 stop_requested 的区别。
void demo_state_matrix() {
    std::stop_source source;
    const std::stop_token token = source.get_token();
    std::printf("[4] fresh stop_source: stop_possible=%d stop_requested=%d\n",
                int(token.stop_possible()), int(token.stop_requested()));

    const bool first = source.request_stop();
    std::printf("[4] first request_stop returned %d, now stop_requested=%d\n",
                int(first), int(token.stop_requested()));

    const bool second = source.request_stop();
    std::printf("[4] second request_stop returned %d\n", int(second));

    // 默认构造的 token 不与任何 source 关联：永远不会被请求，也永远等不到通知。
    const std::stop_token empty;
    std::printf("[4] default stop_token: stop_possible=%d stop_requested=%d\n",
                int(empty.stop_possible()), int(empty.stop_requested()));
}

int main() {
    demo_polling();
    demo_cv_wait();
    demo_callback();
    demo_state_matrix();
}
