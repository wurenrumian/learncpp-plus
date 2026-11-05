// std: c++23
// expect:
// -- one function, three sources --
// c array   : sum=10
// std array : sum=26
// vector    : sum=42
// span<int> is pointer + size, not an element count : true
// span<int>::extent is dynamic_extent (the default) : true
// -- forwarding to a C api --
// fill entire vector: 1 2 3 4 5  (size=5)
// fill first three  : 1 2 3  (size=3)
// -- subviews --
// whole    : 10 20 30 40 50 60  (size=6)
// first(2) : 10 20  (size=2)
// last(2)  : 50 60  (size=2)
// subspan  : 20 30 40  (size=3)
// static 3 : 20 30 40  (size=3)
// static 2 : 50 60  (size=2)
// static extent lives in the type : 2
// -- mdspan 3x4 --
// rank=2 extent(0)=3 extent(1)=4
// layout_right strides: 4 1
// right[0,*]: 0 1 2 3
// right[1,*]: 10 11 12 13
// right[2,*]: 20 21 22 23
// layout_right sum: 138
// layout_left strides: 1 3
// left [0,*]: 0 3 12 21
// left [1,*]: 1 10 13 22
// left [2,*]: 2 11 20 23
// layout_left sum: 138
// same buffer: right[1,0]=10 left[0,1]=3
// dynamic extents: extent(0)=3 extent(1)=4 dyn[2,3]=23
// /expect

#include <array>
#include <cstddef>
#include <cstdio>
#include <mdspan>
#include <span>
#include <vector>

namespace {

// 一个函数，只认「一段 int」，不认容器类型。
int sum(std::span<const int> s) {
    int total = 0;
    for (int x : s) {
        total += x;
    }
    return total;
}

// 老式 C 接口：指针和长度是两个独立参数，配错了编译器不管。
void fill_c(int* p, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        p[i] = static_cast<int>(i + 1);
    }
}

// span 版包装：长度跟着指针走，转发时不会错配。
void fill(std::span<int> s) {
    fill_c(s.data(), s.size());
}

void print_ints(const char* tag, std::span<const int> s) {
    std::printf("%s:", tag);
    for (int x : s) {
        std::printf(" %d", x);
    }
    std::printf("  (size=%zu)\n", s.size());
}

}  // namespace

int main() {
    // 1. 三种来源进同一个函数：C 数组、std::array、std::vector。
    int raw[4] = {1, 2, 3, 4};
    std::array<int, 4> arr{5, 6, 7, 8};
    std::vector<int> vec{9, 10, 11, 12};

    std::printf("-- one function, three sources --\n");
    std::printf("c array   : sum=%d\n", sum(raw));
    std::printf("std array : sum=%d\n", sum(arr));
    std::printf("vector    : sum=%d\n", sum(vec));
    std::printf("span<int> is pointer + size, not an element count : %s\n",
                sizeof(std::span<int>) == sizeof(void*) + sizeof(std::size_t) ? "true" : "false");
    std::printf("span<int>::extent is dynamic_extent (the default) : %s\n",
                std::span<int>::extent == std::dynamic_extent ? "true" : "false");

    // 2. C 风格接口用 span 转发：长度直接来自视图本身。
    std::printf("-- forwarding to a C api --\n");
    std::vector<int> buf(5);
    fill(buf);                                  // vector 隐式转成 span<int>
    print_ints("fill entire vector", buf);
    fill(std::span<int>{buf}.subspan(0, 3));    // 只填前三个
    print_ints("fill first three  ", std::span<const int>{buf}.first(3));

    // 3. 子视图：运行期偏移与编译期偏移两种 subspan。
    std::printf("-- subviews --\n");
    std::vector<int> data{10, 20, 30, 40, 50, 60};
    std::span<int> whole{data};
    print_ints("whole    ", whole);
    print_ints("first(2) ", whole.first(2));
    print_ints("last(2)  ", whole.last(2));

    std::span<int> runtime_mid = whole.subspan(1, 3);      // 偏移、长度都是运行期值
    print_ints("subspan  ", runtime_mid);

    std::span<int, 3> static_mid = whole.subspan<1, 3>();  // extent 进类型
    print_ints("static 3 ", static_mid);

    std::span<int, 2> pair = whole.subspan<4, 2>();        // 静态 extent 的 span
    print_ints("static 2 ", pair);
    std::printf("static extent lives in the type : %zu\n", std::span<int, 2>::extent);

    // 4. mdspan：同一块内存，两种 layout 换的是「索引到偏移」的映射。
    std::printf("-- mdspan 3x4 --\n");
    std::array<int, 12> storage{};

    std::mdspan<int, std::extents<std::size_t, 3, 4>> right{storage.data()};
    for (std::size_t r = 0; r < right.extent(0); ++r) {
        for (std::size_t c = 0; c < right.extent(1); ++c) {
            right[r, c] = static_cast<int>(r * 10 + c);
        }
    }

    std::printf("rank=%zu extent(0)=%zu extent(1)=%zu\n",
                right.rank(), right.extent(0), right.extent(1));
    std::printf("layout_right strides: %zu %zu\n",
                right.mapping().stride(0), right.mapping().stride(1));

    int right_sum = 0;
    for (std::size_t r = 0; r < right.extent(0); ++r) {
        std::printf("right[%zu,*]:", r);
        for (std::size_t c = 0; c < right.extent(1); ++c) {
            std::printf(" %d", right[r, c]);
            right_sum += right[r, c];
        }
        std::printf("\n");
    }
    std::printf("layout_right sum: %d\n", right_sum);

    std::mdspan<int, std::extents<std::size_t, 3, 4>, std::layout_left> left{storage.data()};
    std::printf("layout_left strides: %zu %zu\n",
                left.mapping().stride(0), left.mapping().stride(1));

    int left_sum = 0;
    for (std::size_t r = 0; r < left.extent(0); ++r) {
        std::printf("left [%zu,*]:", r);
        for (std::size_t c = 0; c < left.extent(1); ++c) {
            std::printf(" %d", left[r, c]);
            left_sum += left[r, c];
        }
        std::printf("\n");
    }
    std::printf("layout_left sum: %d\n", left_sum);

    std::printf("same buffer: right[1,0]=%d left[0,1]=%d\n", right[1, 0], left[0, 1]);

    // 同一种 layout，extents 也可以放在运行期：构造参数顺序是 data，然后是各维长度。
    std::mdspan<int, std::dextents<std::size_t, 2>> dyn{storage.data(), 3, 4};
    std::printf("dynamic extents: extent(0)=%zu extent(1)=%zu dyn[2,3]=%d\n",
                dyn.extent(0), dyn.extent(1), dyn[2, 3]);
    return 0;
}
