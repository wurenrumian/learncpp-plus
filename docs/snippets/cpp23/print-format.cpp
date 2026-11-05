// std: c++23
// expect:
// -- integers --
// dec=42 hex=0x2a oct=052 bin=0b101010 plain_bin=101010
// width=    42 zero=000042 sign=+42 space= 42
// -- floating point --
// default=3.14159 fixed=3.14 sci=3.14e+04
// width_and_precision=     3.142 percent=25.0%
// -- strings --
// |left    |   right| center |
// |left****|***right|*center*|
// truncated=form
// -- ranges --
// vector=[1, 2, 3, 5, 8]
// no_brackets=1, 2, 3, 5, 8
// each_hex=[0x0001, 0x0002, 0x0003, 0x0005, 0x0008]
// -- tuple --
// tuple=("Ada", 36, 1.5)
// -- chrono --
// 1970-01-01 was a Thursday
// -- format returns a string --
// id|name|score
// -- vformat for a runtime format string --
// [warn] disk
// /expect

#include <chrono>
#include <format>
#include <print>
#include <string>
#include <tuple>
#include <vector>

int main() {
    std::println("-- integers --");
    std::println("dec={} hex={:#x} oct={:#o} bin={:#b} plain_bin={:b}",
                 42, 42, 42, 42, 42);
    std::println("width={:6} zero={:06} sign={:+} space={: }",
                 42, 42, 42, 42);

    std::println("-- floating point --");
    std::println("default={} fixed={:.2f} sci={:.2e}", 3.14159, 3.14159, 31415.9);
    std::println("width_and_precision={:10.3f} percent={:.1f}%", 3.14159, 25.0);

    std::println("-- strings --");
    std::println("|{:<8}|{:>8}|{:^8}|", "left", "right", "center");
    std::println("|{:*<8}|{:*>8}|{:*^8}|", "left", "right", "center");
    std::println("truncated={:.4}", "format me");

    std::println("-- ranges --");
    const std::vector<int> fib{1, 2, 3, 5, 8};
    std::println("vector={}", fib);
    std::println("no_brackets={:n}", fib);
    std::println("each_hex={::#06x}", fib);

    std::println("-- tuple --");
    const auto entry = std::tuple{std::string{"Ada"}, 36, 1.5};
    std::println("tuple={}", entry);

    std::println("-- chrono --");
    using namespace std::chrono;
    const sys_days epoch{1970y / January / 1d};
    std::println("{:%Y-%m-%d} was a {:%A}", epoch, weekday{epoch});

    std::println("-- format returns a string --");
    const std::string header = std::format("{}|{}|{}", "id", "name", "score");
    std::println("{}", header);

    std::println("-- vformat for a runtime format string --");
    const std::string user_fmt = "[{}] {}";
    std::println("{}", std::vformat(user_fmt, std::make_format_args("warn", "disk")));

    static_assert(std::formattable<std::vector<int>, char>);
    return 0;
}
