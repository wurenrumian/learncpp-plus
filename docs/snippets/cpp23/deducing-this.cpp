// std: c++23
// expect:
// [lvalue] name=widget
// [const lvalue] name=const widget
// [rvalue] name=widget
// static_asserts passed: Widget& / const Widget& / Widget&&
// released: widget
// shape: circle
// fib(20) = 6765
// /expect

#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// 把一个值类别翻译成可读的名字，方便观察 Self 到底推导成了什么。
template <class Self>
constexpr std::string_view category_name() {
    if constexpr (std::is_rvalue_reference_v<Self&&>) {
        return "rvalue";
    } else if constexpr (std::is_const_v<std::remove_reference_t<Self>>) {
        return "const lvalue";
    } else {
        return "lvalue";
    }
}

struct Widget {
    std::string name{"widget"};

    // 一个模板覆盖 const / 非 const / 左值 / 右值四种情形。
    // 返回类型 auto&& 跟着 Self 走：Widget& -> string&，
    // const Widget& -> const string&，Widget&& -> string&&。
    template <class Self>
    constexpr auto&& at(this Self&& self) noexcept {
        return std::forward<Self>(self).name;
    }

    // 同一个函数体，用 if constexpr 针对 Self 分派行为。
    template <class Self>
    void describe(this Self&& self) {
        std::println("[{}] name={}", category_name<Self>(), std::forward<Self>(self).name);
    }

    // 只允许右值调用：以前要写一个单独的 && 限定重载，
    // 现在用 static_assert 把约束写进同一个函数体。
    template <class Self>
    std::string release(this Self&& self) {
        static_assert(std::is_rvalue_reference_v<Self&&>, "release() requires an rvalue");
        return std::move(self.name);
    }
};

// 值类别与返回类型在编译期就能检查，不需要跑到运行时。
static_assert(std::is_same_v<decltype(std::declval<Widget&>().at()), std::string&>);
static_assert(std::is_same_v<decltype(std::declval<const Widget&>().at()), const std::string&>);
static_assert(std::is_same_v<decltype(std::declval<Widget&&>().at()), std::string&&>);

// 用 deducing this 替掉 CRTP：不需要模板基类，也不需要 static_cast。
struct Drawable {
    template <class Self>
    void print(this Self&& self) {
        // Self 推导成 Circle& / const Circle&，静态分派到派生类的实现。
        std::println("shape: {}", std::forward<Self>(self).shape_name());
    }
};

struct Circle : Drawable {
    std::string_view shape_name() const { return "circle"; }
};

int main() {
    Widget w;
    const Widget cw{"const widget"};

    w.describe();
    cw.describe();
    std::move(w).describe();

    std::println("static_asserts passed: Widget& / const Widget& / Widget&&");

    // 右值访问可以真正把成员搬走，而且没有为它写第二个重载。
    std::println("released: {}", std::move(w).release());

    Circle c;
    c.print();

    // 递归 lambda：显式对象参数让 lambda 能把自己当作参数传下去。
    auto fib = [](this auto&& self, int n) -> long long {
        return n < 2 ? n : self(n - 1) + self(n - 2);
    };
    std::println("fib(20) = {}", fib(20));
}
