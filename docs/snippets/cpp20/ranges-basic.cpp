// std: c++23
// expect:
// build pipeline   : transform calls = 0
// one pass         : sum = 56, transform calls = 3
// odd numbers desc : 5 3 1
// materialized size: 3
// materialized     : 30 60 90
// c++20 fallback ok: true
// /expect
#include <print>
#include <ranges>
#include <vector>

int main() {
    std::vector<int> xs{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // 1. 组装管道：此时什么也没算，transform 一次都没被调用。
    int transform_calls = 0;
    auto numbers = xs
        | std::views::filter([](int x) { return x % 2 == 0; })
        | std::views::transform([&transform_calls](int x) {
              ++transform_calls;
              return x * x;
          })
        | std::views::take(3);

    std::println("build pipeline   : transform calls = {}", transform_calls);

    // 2. 迭代一次：只拉取需要的前 3 个，之后就不再上游取数。
    long long sum = 0;
    for (int x : numbers) {
        sum += x;
    }
    std::println("one pass         : sum = {}, transform calls = {}", sum, transform_calls);

    // 3. drop / reverse 同样是惰性适配器，可以接在同一根管道上。
    std::print("odd numbers desc :");
    for (int x : xs
        | std::views::filter([](int x) { return x % 2 != 0; })
        | std::views::reverse
        | std::views::drop(2)) {
        std::print(" {}", x);
    }
    std::print("\n");

    // 4. 材料化：C++23 的 ranges::to 一步落成 vector。
    auto tripled = xs
        | std::views::filter([](int x) { return x % 3 == 0; })
        | std::views::transform([](int x) { return x * 10; })
        | std::ranges::to<std::vector<int>>();

    std::println("materialized size: {}", tripled.size());
    std::print("materialized     :");
    for (int x : tripled) {
        std::print(" {}", x);
    }
    std::print("\n");

    // 5. C++20 没有 ranges::to，用迭代器对构造是等价写法。
    std::vector<int> const same(tripled.begin(), tripled.end());
    std::println("c++20 fallback ok: {}", same == tripled);
}
