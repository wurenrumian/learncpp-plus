// std: c++20
// expect:
// HasSize<string> = true
// HasSize<int> = false
// Numeric<int> = true
// Numeric<string> = false
// twice(21) = 42
// twice(1.5) = 3
// describe: 3 items
// kind(7) = integral
// kind(2.5) = numeric
// box holds 42
// print_num: 7
// /expect

#include <concepts>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

// 1. 自定义 concept：有 size()，且结果能转成 size_t。
template <typename T>
concept HasSize = requires(const T& value) {
    { value.size() } -> std::convertible_to<std::size_t>;
};

// 2. 用逻辑或把标准 concept 拼起来。
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

// 3. concept 在编译期求值，可以直接拿来做断言。
static_assert(HasSize<std::string>);
static_assert(HasSize<std::vector<int>>);
static_assert(!HasSize<int>);
static_assert(Numeric<int>);
static_assert(Numeric<double>);
static_assert(!Numeric<std::string>);

// requires 表达式本身就是 bool 常量表达式。
static_assert(requires(int a, int b) { a + b; });

// 4. 用 concept 约束函数模板。
template <typename T>
    requires Numeric<T>
T twice(T value) {
    return value + value;
}

// 5. 另一种约束写法：把 concept 直接写在 template 参数表里。
template <HasSize T>
void describe(const T& container) {
    std::cout << "describe: " << container.size() << " items\n";
}

// 6. 缩写函数模板：auto 前面挂 concept，等价于隐式模板参数。
void print_num(std::integral auto value) {
    std::cout << "print_num: " << value << '\n';
}

// 7. 用 concept 约束类模板。
template <Numeric T>
class Box {
public:
    explicit Box(T value) : value_(value) {}
    T get() const { return value_; }

private:
    T value_;
};

// 8. 重载 + subsumption：更受约束的版本优先。
template <typename T>
    requires Numeric<T>
std::string kind(T) {
    return "numeric";
}

template <typename T>
    requires Numeric<T> && std::integral<T>
std::string kind(T) {
    return "integral";
}

int main() {
    std::cout << std::boolalpha;
    std::cout << "HasSize<string> = " << HasSize<std::string> << '\n';
    std::cout << "HasSize<int> = " << HasSize<int> << '\n';
    std::cout << "Numeric<int> = " << Numeric<int> << '\n';
    std::cout << "Numeric<string> = " << Numeric<std::string> << '\n';

    std::cout << "twice(21) = " << twice(21) << '\n';
    std::cout << "twice(1.5) = " << twice(1.5) << '\n';

    std::vector<int> items{1, 2, 3};
    describe(items);

    std::cout << "kind(7) = " << kind(7) << '\n';
    std::cout << "kind(2.5) = " << kind(2.5) << '\n';

    std::cout << "box holds " << Box<int>{42}.get() << '\n';
    print_num(7);
}
