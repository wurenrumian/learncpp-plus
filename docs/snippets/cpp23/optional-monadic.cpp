// std: c++23
// expect:
// -- 1. success path: parse, double, describe --
// input "8080"
//   and_then: port 8080 -> 16160
//   transform: 16160 -> text
//   result: "port 16160 (listen ok)"
// -- 2. and_then rejects the value --
// input "40000"
//   and_then: port 40000 -> out of range after doubling
//   transform: lambda not run, chain is already empty
//   result: <empty>
// -- 3. parse_port fails: trailing garbage --
// input "80ab"
//   parse_port: <empty>, so the two lambdas never run
//   result: <empty>
// -- 4. parse_port fails: out of range --
// input "70000"
//   parse_port: <empty>, so the two lambdas never run
//   result: <empty>
// -- 5. or_else --
// or_else on empty: 443 (lambda called 1 time)
// or_else on value: 8080 (lambda called 0 times)
// -- 6. value_or is eager --
//   fallback_port() called
// value_or on value 8080 -> 8080
//   fallback_port() called
// value_or on empty -> 1
// /expect

#include <charconv>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

// 把整段文本解析成端口：必须全是数字，且落在 [1, 65535]。
std::optional<int> parse_port(std::string_view text) {
    if (text.empty())
        return std::nullopt;

    int value = 0;
    const char* first = text.data();
    const char* last = first + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) // 不是数字，或数字后面还有残余字符
        return std::nullopt;
    if (value < 1 || value > 65535) // 越界
        return std::nullopt;
    return value;
}

// 不会抛、也不会失败的纯函数：适合放进 transform。
std::string describe(int port) {
    return "port " + std::to_string(port) + " (listen ok)";
}

// 带副作用的兜底值，用来证明 value_or 的实参总会被求值。
int fallback_port() {
    std::printf("  fallback_port() called\n");
    return 1;
}

void run_case(std::string_view input) {
    std::printf("input \"%.*s\"\n", static_cast<int>(input.size()), input.data());

    const std::optional<int> parsed = parse_port(input);
    if (!parsed)
        std::printf("  parse_port: <empty>, so the two lambdas never run\n");

    const std::optional<std::string> result =
        parsed
            .and_then([](int port) -> std::optional<int> {
                if (port > 32767) {
                    std::printf("  and_then: port %d -> out of range after doubling\n", port);
                    return std::nullopt; // 这一步失败，后面的 transform 不会跑
                }
                std::printf("  and_then: port %d -> %d\n", port, port * 2);
                return port * 2;
            })
            .transform([](int port) {
                std::printf("  transform: %d -> text\n", port);
                return describe(port);
            });

    if (parsed && !result)
        std::printf("  transform: lambda not run, chain is already empty\n");

    if (result)
        std::printf("  result: \"%s\"\n", result->c_str());
    else
        std::printf("  result: <empty>\n");
}

int main() {
    std::printf("-- 1. success path: parse, double, describe --\n");
    run_case("8080");

    std::printf("-- 2. and_then rejects the value --\n");
    run_case("40000");

    std::printf("-- 3. parse_port fails: trailing garbage --\n");
    run_case("80ab");

    std::printf("-- 4. parse_port fails: out of range --\n");
    run_case("70000");

    std::printf("-- 5. or_else --\n");
    int fallback_calls = 0;
    const std::optional<int> rescued = parse_port("abc").or_else([&fallback_calls] {
        ++fallback_calls;
        return std::optional<int>{443};
    });
    std::printf("or_else on empty: %d (lambda called %d time)\n", rescued.value_or(-1), fallback_calls);

    fallback_calls = 0;
    const std::optional<int> untouched = parse_port("8080").or_else([&fallback_calls] {
        ++fallback_calls;
        return std::optional<int>{443};
    });
    std::printf("or_else on value: %d (lambda called %d times)\n", untouched.value_or(-1), fallback_calls);

    std::printf("-- 6. value_or is eager --\n");
    const int keeps = parse_port("8080").value_or(fallback_port());
    std::printf("value_or on value 8080 -> %d\n", keeps);
    const int rescued_int = parse_port("0").value_or(fallback_port());
    std::printf("value_or on empty -> %d\n", rescued_int);

    return 0;
}
