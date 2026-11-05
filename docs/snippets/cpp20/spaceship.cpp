// std: c++20
// expect:
// Point is three_way_comparable: true
// (1,2) <=> (1,3) is less
// (2,0) <=> (2,0) is equal
// sorted points: (1,2) (1,3) (2,0) (2,5)
// Name Alice <=> alice is equivalent
// Name Alice == alice is false
// sorted names: alice Bob Carol
// 1.0 <=> nan is unordered
// nan <=> nan is unordered
// 1.0 <=> 2.0 is less
// /expect

#include <algorithm>
#include <compare>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

// 1. 默认的 <=>：按声明顺序逐成员比较，得到强序。
struct Point {
    int x;
    int y;

    auto operator<=>(const Point&) const = default;
};

static_assert(std::three_way_comparable<Point>);

// 2. 手写弱序：忽略大小写的比较，和区分大小写的 == 并存。
class Name {
public:
    explicit Name(std::string text) : text_(text) {}

    std::weak_ordering operator<=>(const Name& other) const {
        return compare_ignoring_case(text_, other.text_);
    }

    bool operator==(const Name& other) const { return text_ == other.text_; }

    const std::string& text() const { return text_; }

private:
    static char lower(char c) {
        if (c >= 'A' && c <= 'Z') {
            return static_cast<char>(c - 'A' + 'a');
        }
        return c;
    }

    static std::weak_ordering compare_ignoring_case(const std::string& a,
                                                    const std::string& b) {
        const std::size_t common = a.size() < b.size() ? a.size() : b.size();
        for (std::size_t i = 0; i < common; ++i) {
            const char ca = lower(a[i]);
            const char cb = lower(b[i]);
            if (ca < cb) {
                return std::weak_ordering::less;
            }
            if (cb < ca) {
                return std::weak_ordering::greater;
            }
        }
        if (a.size() < b.size()) {
            return std::weak_ordering::less;
        }
        if (b.size() < a.size()) {
            return std::weak_ordering::greater;
        }
        return std::weak_ordering::equivalent;
    }

    std::string text_;
};

// 把一个 ordering 结果翻译成可打印的 ASCII 单词。
const char* describe(std::strong_ordering order) {
    if (order == std::strong_ordering::less) {
        return "less";
    }
    if (order == std::strong_ordering::greater) {
        return "greater";
    }
    return "equal";
}

const char* describe(std::weak_ordering order) {
    if (order == std::weak_ordering::less) {
        return "less";
    }
    if (order == std::weak_ordering::greater) {
        return "greater";
    }
    return "equivalent";
}

const char* describe(std::partial_ordering order) {
    if (order == std::partial_ordering::less) {
        return "less";
    }
    if (order == std::partial_ordering::greater) {
        return "greater";
    }
    if (order == std::partial_ordering::equivalent) {
        return "equivalent";
    }
    return "unordered";
}

int main() {
    std::printf("Point is three_way_comparable: %s\n",
                std::three_way_comparable<Point> ? "true" : "false");

    const Point a{1, 2};
    const Point b{1, 3};
    std::printf("(1,2) <=> (1,3) is %s\n", describe(a <=> b));

    const Point c{2, 0};
    std::printf("(2,0) <=> (2,0) is %s\n", describe(c <=> c));

    std::vector<Point> points{{2, 5}, {1, 3}, {2, 0}, {1, 2}};
    std::sort(points.begin(), points.end());
    std::printf("sorted points:");
    for (const Point& p : points) {
        std::printf(" (%d,%d)", p.x, p.y);
    }
    std::printf("\n");

    const Name alice{"Alice"};
    const Name alice_lower{"alice"};
    std::printf("Name Alice <=> alice is %s\n", describe(alice <=> alice_lower));
    std::printf("Name Alice == alice is %s\n",
                static_cast<bool>(alice == alice_lower) ? "true" : "false");

    std::vector<Name> names{Name{"Carol"}, Name{"alice"}, Name{"Bob"}};
    std::sort(names.begin(), names.end());
    std::printf("sorted names:");
    for (const Name& name : names) {
        std::printf(" %s", name.text().c_str());
    }
    std::printf("\n");

    const double one = 1.0;
    const double two = 2.0;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::printf("1.0 <=> nan is %s\n", describe(one <=> nan));
    std::printf("nan <=> nan is %s\n", describe(nan <=> nan));
    std::printf("1.0 <=> 2.0 is %s\n", describe(one <=> two));
}
