// std: c++20
// expect:
// == lazy task ==
// after create: suspends=2 resumes=0
// [add_one] body entered
// [one] body entered
// [add_one] co_await returned
// after resume: suspends=5 resumes=3 value=42
// == generator ==
// after create: suspends=6 resumes=3
// [gen] body entered
// yield #1 -> 1 (suspends=7 resumes=4)
// yield #2 -> 2 (suspends=8 resumes=5)
// yield #3 -> 3 (suspends=9 resumes=6)
// [gen] body finished
// after loop: suspends=10 resumes=7
// /expect

#include <coroutine>
#include <cstdio>
#include <exception>
#include <utility>

namespace {

// 两个计数器：只为了让输出的数字证明"什么时候才开始执行"。
int g_suspends = 0; // 挂起次数
int g_resumes = 0;  // 恢复（真正进入协程体继续执行）次数

// 每个挂起点都用它计数。await_ready 返回 false = 一定挂起。
struct counted_suspend {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<>) const noexcept { ++g_suspends; }
    void await_resume() const noexcept {}
};

// 结束挂起点的 awaiter：把控制权交还给调用者（symmetric transfer）。
// continuation 为空时交给 noop_coroutine，等于直接返回 resume() 的调用点。
struct resume_continuation {
    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        ++g_suspends; // 结束挂起点同样是一次挂起
        std::coroutine_handle<> next = handle.promise().continuation;
        if (next) {
            ++g_resumes;
            return next;
        }
        return std::noop_coroutine();
    }

    void await_resume() const noexcept {}
};

// 惰性 task<T>：创建时不执行，resume() 才执行。
template <typename T>
class task {
public:
    struct promise_type {
        T value{};
        std::coroutine_handle<> continuation{};

        task get_return_object() {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        counted_suspend initial_suspend() const noexcept { return {}; } // 惰性的关键
        resume_continuation final_suspend() const noexcept { return {}; }

        void return_value(T v) { value = std::move(v); }
        void unhandled_exception() const noexcept { std::terminate(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    task() noexcept = default;
    task(task&& other) noexcept : handle_(other.handle_) { other.handle_ = {}; }
    task& operator=(task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = other.handle_;
            other.handle_ = {};
        }
        return *this;
    }
    task(const task&) = delete;
    task& operator=(const task&) = delete;

    ~task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    void resume() {
        if (done()) {
            return;
        }
        ++g_resumes;
        handle_.resume();
    }

    bool done() const noexcept { return !handle_ || handle_.done(); }

    T value() {
        return std::move(handle_.promise().value);
    }

    // co_await other_task 用的 awaiter：把对方的 continuation 指向自己，
    // 再把控制权直接交给对方（而不是返回调用栈）。
    struct awaiter {
        handle_type inner{};

        bool await_ready() const noexcept { return false; }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
            ++g_suspends; // 自己被这个 co_await 挂起
            inner.promise().continuation = caller;
            ++g_resumes; // 控制权交给 inner，等于被恢复
            return inner;
        }

        T await_resume() {
            return std::move(inner.promise().value);
        }
    };

    awaiter operator co_await() && noexcept {
        return awaiter{handle_};
    }

private:
    explicit task(handle_type handle) noexcept : handle_(handle) {}

    handle_type handle_{};
};

// 手写 generator<T>：co_yield 把值存进 promise 后挂起。
template <typename T>
class generator {
public:
    struct promise_type {
        T current{};

        generator get_return_object() {
            return generator{handle_type::from_promise(*this)};
        }

        counted_suspend initial_suspend() const noexcept { return {}; }
        counted_suspend final_suspend() const noexcept { return {}; }

        counted_suspend yield_value(T v) {
            current = std::move(v);
            return {};
        }

        void return_void() const noexcept {}
        void unhandled_exception() const noexcept { std::terminate(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    generator() noexcept = default;
    generator(generator&& other) noexcept : handle_(other.handle_) { other.handle_ = {}; }
    generator(const generator&) = delete;
    generator& operator=(const generator&) = delete;

    ~generator() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // 驱动一步：body 从上次挂起点继续跑到下一个 co_yield 或结束。
    bool next() {
        if (!handle_ || handle_.done()) {
            return false;
        }
        ++g_resumes;
        handle_.resume();
        return !handle_.done();
    }

    const T& value() const noexcept { return handle_.promise().current; }

private:
    explicit generator(handle_type handle) noexcept : handle_(handle) {}

    handle_type handle_{};
};

// ---- 被驱动的协程 ----

task<int> one() {
    std::puts("[one] body entered");
    co_return 41; // 不挂起，直接进结束挂起点
}

task<int> add_one(task<int> inner) {
    std::puts("[add_one] body entered");
    int v = co_await std::move(inner); // 挂起自己，把控制权交给 inner
    std::puts("[add_one] co_await returned");
    co_return v + 1;
}

generator<int> count_up() {
    std::puts("[gen] body entered");
    co_yield 1;
    co_yield 2;
    co_yield 3;
    std::puts("[gen] body finished");
}

} // namespace

int main() {
    std::puts("== lazy task ==");
    auto outer = add_one(one());
    std::printf("after create: suspends=%d resumes=%d\n", g_suspends, g_resumes);

    outer.resume();
    std::printf("after resume: suspends=%d resumes=%d value=%d\n",
                g_suspends, g_resumes, outer.value());

    std::puts("== generator ==");
    auto gen = count_up();
    std::printf("after create: suspends=%d resumes=%d\n", g_suspends, g_resumes);

    int step = 0;
    while (gen.next()) {
        ++step;
        std::printf("yield #%d -> %d (suspends=%d resumes=%d)\n",
                    step, gen.value(), g_suspends, g_resumes);
    }
    std::printf("after loop: suspends=%d resumes=%d\n", g_suspends, g_resumes);
}
