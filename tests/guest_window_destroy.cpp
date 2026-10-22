#include "../android/app/src/main/cpp/guest_window_destroy.h"
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <utility>
#include <vector>

struct Window { uint32_t parent; };
int main() {
    std::map<uint32_t, Window> windows{{1, {0}}, {2, {1}}, {3, {2}}, {4, {1}}, {5, {0}}};
    std::set<uint32_t> destroying;
    std::vector<std::pair<uint32_t,uint32_t>> messages;
    std::vector<uint32_t> cleanupOrder;
    std::function<void(uint32_t,uint32_t)> onMessage;
    auto onCleanup=[&](uint32_t h){ cleanupOrder.push_back(h); };
    onMessage=[&](uint32_t h,uint32_t message) {
        messages.emplace_back(h,message);
        if(h==1 && message==2)
            assert(!destroyGuestWindowTree(windows,destroying,1,onMessage,onCleanup));
    };
    assert(!destroyGuestWindowTree(windows,destroying,999,onMessage,onCleanup));
    assert(destroyGuestWindowTree(windows,destroying,1,onMessage,onCleanup));
    assert(windows.size()==1 && windows.count(5));
    assert((messages==std::vector<std::pair<uint32_t,uint32_t>>{
        {1,2},{2,2},{3,2},{3,0x82},{2,0x82},{4,2},{4,0x82},{1,0x82}
    }));
    assert((cleanupOrder==std::vector<uint32_t>{3,2,4,1}));
    assert(destroying.empty());
    assert(!destroyGuestWindowTree(windows,destroying,1,onMessage,onCleanup));
    assert(messages.size()==8);
    std::cout << "guest DestroyWindow recursion and cleanup PASS\n";
}
