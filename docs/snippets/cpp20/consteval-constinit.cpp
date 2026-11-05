// std: c++23
// expect:
// fnv1a("learncpp") runtime     = 2856973362
// const_hash("learncpp") compile = 2856973362
// where() compile-time branch   = compile-time
// where() runtime branch        = runtime
// constinit global start        = 0
// constinit global after += 7   = 7
// constinit static start        = 100
// constinit static after += 3   = 103
// constinit static after += 5   = 108
// constexpr twice(21) runtime   = 42
// constexpr twice(21) compile   = 42
// /expect

#include <cstddef>
#include <cstdio>
#include <string_view>

// 1. FNV-1a（32 位）。constexpr 版本在编译期和运行期都能用。
constexpr unsigned fnv1a(std::string_view text) {
    unsigned hash = 2166136261u;   // offset basis
    for (char ch : text) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 16777619u;         // FNV prime
    }
    return hash;
}

// 2. 立即函数：包一层，明确"只许在编译期求值"。
consteval unsigned const_hash(std::string_view text) {
    return fnv1a(text);
}

// 3. 编译期真的算了一遍：算错就编译失败，不是等到运行期。
constexpr unsigned learncpp_hash = const_hash("learncpp");
static_assert(learncpp_hash == 2856973362u);

// 4. if consteval：同一函数，编译期与运行期走不同实现。
constexpr const char* where() {
    if consteval {
        return "compile-time";
    } else {
        return "runtime";
    }
}

constexpr const char* compiled_at = where();
static_assert(std::string_view{compiled_at} == "compile-time");

// 5. constinit：只保证常量初始化，变量本身仍然可写。
constinit int global_hits = 0;

constexpr int kStaticStart = 100;

int bump_static(int step) {
    static constinit int local_hits = kStaticStart;
    local_hits += step;
    return local_hits;
}

// 6. constexpr 函数：编译期能算，运行期也能调用（may，不是 must）。
constexpr int twice(int value) { return value * 2; }
constexpr int twice_at_compile = twice(21);
static_assert(twice_at_compile == 42);

int main() {
    std::printf("fnv1a(\"learncpp\") runtime     = %u\n", fnv1a("learncpp"));
    std::printf("const_hash(\"learncpp\") compile = %u\n", learncpp_hash);

    std::printf("where() compile-time branch   = %s\n", compiled_at);
    std::printf("where() runtime branch        = %s\n", where());

    std::printf("constinit global start        = %d\n", global_hits);
    global_hits += 7;
    std::printf("constinit global after += 7   = %d\n", global_hits);

    std::printf("constinit static start        = %d\n", kStaticStart);
    std::printf("constinit static after += 3   = %d\n", bump_static(3));
    std::printf("constinit static after += 5   = %d\n", bump_static(5));

    int input = 21;
    std::printf("constexpr twice(21) runtime   = %d\n", twice(input));
    std::printf("constexpr twice(21) compile   = %d\n", twice_at_compile);
}
