// std: c++23
// expect:
// [1] input "8080"
//     parse ok -> 8080
//     and_then ok -> 16160
//     transform ok -> 16162
//     result: value 16162
// [2] input "60000"
//     parse ok -> 60000
//     and_then failed -> cannot double 60000 without overflow
//     transform skipped
//     result: error "cannot double 60000 without overflow", value_or(1) = 1
// [3] input "70000"
//     parse failed -> port out of range: 70000
//     chain short-circuited, then/transform never ran
//     result: error "port out of range: 70000", value_or(1) = 1
// [4] input "80a0"
//     parse failed -> not all digits: 80a0
//     chain short-circuited, then/transform never ran
//     result: error "not all digits: 80a0", value_or(1) = 1
// [5] input ""
//     parse failed -> empty input
//     chain short-circuited, then/transform never ran
//     result: error "empty input", value_or(1) = 1
// final line: 4 failures out of 5
// bad_expected_access caught: has_value() was false
// expected<void, E>: ok
// expected<void, E>: error "disk full"
// /expect
#include <expected>
#include <print>
#include <string>
#include <string_view>

// 解析端口号：只接受 1..65535 的十进制数字串。
// 失败是"预期的"——用户输入本来就可能非法，所以写进返回类型而不是抛异常。
std::expected<int, std::string> parse_port(std::string_view text) {
    if (text.empty())
        return std::unexpected("empty input");

    int value = 0;
    for (char c : text) {
        if (c < '0' || c > '9')
            return std::unexpected("not all digits: " + std::string(text));
        value = value * 10 + (c - '0');
        if (value > 65535)  // 早退，避免 int 溢出
            return std::unexpected("port out of range: " + std::string(text));
    }
    return value;
}

// 链路上的第二步：也可能失败，错误类型同样是 std::string。
std::expected<int, std::string> double_port(int port) {
    if (port > 32767)
        return std::unexpected("cannot double " + std::to_string(port) + " without overflow");
    return port * 2;
}

std::expected<void, std::string> write_file(bool ok) {
    if (!ok)
        return std::unexpected("disk full");
    return {};
}

int main() {
    const std::string_view inputs[] = {"8080", "60000", "70000", "80a0", ""};
    int failures = 0;

    for (std::size_t i = 0; i < std::size(inputs); ++i) {
        const std::string_view input = inputs[i];
        std::println("[{}] input \"{}\"", i + 1, input);

        bool reached_and_then = false;
        bool reached_transform = false;

        // and_then：接一个"返回 expected 的函数"，成功才调用，失败直接透传。
        // transform：接一个"返回普通值的函数"，把成功值映射成新值，不会新增失败。
        auto result = parse_port(input)
                          .and_then([&](int port) {
                              reached_and_then = true;
                              std::println("    parse ok -> {}", port);
                              return double_port(port);
                          })
                          .transform([&](int port) {
                              reached_transform = true;
                              std::println("    and_then ok -> {}", port);
                              return port + 2;
                          });

        if (result.has_value()) {
            std::println("    transform ok -> {}", result.value());
            std::println("    result: value {}", result.value());
        } else {
            ++failures;
            if (!reached_and_then) {
                std::println("    parse failed -> {}", result.error());
                std::println("    chain short-circuited, then/transform never ran");
            } else if (!reached_transform) {
                std::println("    and_then failed -> {}", result.error());
                std::println("    transform skipped");
            }
            // error()：无值时返回 E；value_or()：无值时给兜底值，绝不抛异常。
            std::println("    result: error \"{}\", value_or(1) = {}",
                         result.error(), result.value_or(1));
        }
    }

    std::println("final line: {} failures out of 5", failures);

    // 常见坑：无值时调用 value() 会抛 std::bad_expected_access<E>。
    std::expected<int, std::string> missing = std::unexpected("nope");
    try {
        (void)missing.value();
        std::println("bad_expected_access NOT thrown");
    } catch (const std::bad_expected_access<std::string>&) {
        std::println("bad_expected_access caught: has_value() was false");
    }

    // expected<void, E> 用空的花括号表示成功，没有任何占位值。
    auto ok = write_file(true);
    std::println("expected<void, E>: {}", ok ? "ok" : "error");
    auto bad = write_file(false);
    if (!bad)
        std::println("expected<void, E>: error \"{}\"", bad.error());
}
