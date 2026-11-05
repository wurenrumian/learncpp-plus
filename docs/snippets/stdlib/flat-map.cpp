// std: c++23
// expect:
// [1] inserted 6 keys, size = 6
//     apple   3
//     banana  2
//     cherry  9
//     date    4
//     kiwi    5
//     pear    7
// [2] find("cherry") hit, value = 9
//     find("grape") miss, no such key
// [3] try_emplace("apple", 999): inserted = 0, value = 3
// [4] after operator[]: size = 7
//     apple   42
//     banana  2
//     cherry  9
//     date    4
//     fig     1
//     kiwi    5
//     pear    7
// [5] erase("date") removed 1, size = 6
//     erase(begin()) -> next key is banana, size = 5
// [6] batch build: rows after sort+unique = 5, flat_map size = 5
//     keys()   = apple banana cherry kiwi pear
//     values() = 3 2 9 5 7
//     plain ctor from unsorted raw: size = 3, order = apple=3 banana=2 kiwi=5
// [7] flat_set: 3 unique tags: alpha beta gamma
//     contains("beta") = 1, contains("delta") = 0
// /expect
#include <algorithm>
#include <cstdio>
#include <flat_map>
#include <flat_set>
#include <string>
#include <utility>
#include <vector>

int main() {
    // ---- [1] 逐条插入 6 个键：遍历顺序按键排序，与插入顺序无关 ----
    std::flat_map<std::string, int> stock;
    stock.try_emplace("pear", 7);
    stock.try_emplace("apple", 3);
    stock.try_emplace("kiwi", 5);
    stock.try_emplace("banana", 2);
    stock.try_emplace("cherry", 9);
    stock.try_emplace("date", 4);

    std::printf("[1] inserted 6 keys, size = %zu\n", stock.size());
    for (const auto& [k, v] : stock)
        std::printf("    %-7s %d\n", k.c_str(), v);

    // ---- [2] find：命中与未命中 ----
    const auto hit = stock.find("cherry");
    if (hit != stock.end())
        std::printf("[2] find(\"cherry\") hit, value = %d\n", hit->second);
    else
        std::printf("[2] find(\"cherry\") miss\n");

    if (stock.find("grape") == stock.end())
        std::printf("    find(\"grape\") miss, no such key\n");
    else
        std::printf("    find(\"grape\") hit\n");

    // ---- [3] try_emplace 对已存在的 key 不覆盖 ----
    const auto [it_apple, inserted] = stock.try_emplace("apple", 999);
    std::printf("[3] try_emplace(\"apple\", 999): inserted = %d, value = %d\n",
                inserted ? 1 : 0, it_apple->second);

    // ---- [4] operator[]：已存在就改 value，不存在就插入（可能重排、重新分配）----
    stock["apple"] = 42;  // apple 已存在：直接写 value
    stock["fig"] = 1;     // fig 不存在：插入到排序位置
    std::printf("[4] after operator[]: size = %zu\n", stock.size());
    for (const auto& [k, v] : stock)
        std::printf("    %-7s %d\n", k.c_str(), v);

    // ---- [5] erase：同一个名字，两个重载，返回值不同 ----
    const std::size_t removed = stock.erase("date");  // 按 key：返回删掉的个数
    std::printf("[5] erase(\"date\") removed %zu, size = %zu\n", removed, stock.size());

    const auto next = stock.erase(stock.begin());  // 按 iterator：返回下一个位置
    std::printf("    erase(begin()) -> next key is %s, size = %zu\n",
                next->first.c_str(), stock.size());

    // ---- [6] 批量构建：先排好序，再一次性交给容器 ----
    std::vector<std::pair<std::string, int>> rows{
        {"pear", 7}, {"apple", 3}, {"kiwi", 5}, {"banana", 2}, {"apple", 3}, {"cherry", 9},
    };
    std::sort(rows.begin(), rows.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    // sorted_unique 是"我保证按 key 升序且无重复"的承诺，容器不检查，所以先自己去重。
    const auto last = std::unique(rows.begin(), rows.end(),
                                  [](const auto& a, const auto& b) { return a.first == b.first; });
    rows.erase(last, rows.end());

    std::flat_map<std::string, int> batch(std::sorted_unique, rows.begin(), rows.end());
    std::printf("[6] batch build: rows after sort+unique = %zu, flat_map size = %zu\n",
                rows.size(), batch.size());

    // keys() / values() 是两块并行的连续数组，可以直接读或拿去算法里用。
    std::printf("    keys()   =");
    for (const auto& k : batch.keys())
        std::printf(" %s", k.c_str());
    std::printf("\n    values() =");
    for (const int v : batch.values())
        std::printf(" %d", v);
    std::printf("\n");

    // 不给 sorted_unique，容器自己排序 + 去重，语义与逐条插入一致（首次出现的那个值留下）。
    const std::vector<std::pair<std::string, int>> raw{
        {"kiwi", 5}, {"apple", 3}, {"kiwi", 5}, {"banana", 2},
    };
    const std::flat_map<std::string, int> plain(raw.begin(), raw.end());
    std::printf("    plain ctor from unsorted raw: size = %zu, order =", plain.size());
    for (const auto& [k, v] : plain)
        std::printf(" %s=%d", k.c_str(), v);
    std::printf("\n");

    // ---- [7] flat_set：key 就是 value，插入时自动去重 ----
    std::flat_set<std::string> tags;
    for (const std::string& t : {"beta", "alpha", "gamma", "alpha", "beta"})
        tags.insert(t);

    std::printf("[7] flat_set: %zu unique tags:", tags.size());
    for (const auto& t : tags)
        std::printf(" %s", t.c_str());
    std::printf("\n    contains(\"beta\") = %d, contains(\"delta\") = %d\n",
                tags.contains("beta") ? 1 : 0, tags.contains("delta") ? 1 : 0);
}
