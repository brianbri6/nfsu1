#pragma once
// Win32-style recursive teardown of guest windows.
// Kept independent of Android and of the recompilation runtime so that
// callback and lifetime behavior can be regression-tested on the host.
#include <cstdint>
#include <set>
#include <vector>

template<class WindowMap, class OnMessage, class OnCleanup>
bool destroyGuestWindowTree(WindowMap& windows, std::set<uint32_t>& destroying,
                            uint32_t handle, OnMessage& onMessage, OnCleanup& onCleanup) {
    auto current=windows.find(handle);
    if(current==windows.end()||!destroying.insert(handle).second) return false;
    // Windows dispatches WM_DESTROY before WM_NCDESTROY. Child windows must
    // be destroyed before their parent's final nonclient teardown.
    onMessage(handle,0x0002u); // WM_DESTROY
    std::vector<uint32_t> children;
    for(const auto& pair:windows)
        if(pair.first!=handle&&pair.second.parent==handle) children.push_back(pair.first);
    for(uint32_t child:children)
        destroyGuestWindowTree(windows,destroying,child,onMessage,onCleanup);
    onMessage(handle,0x0082u); // WM_NCDESTROY
    onCleanup(handle);
    windows.erase(handle);
    destroying.erase(handle);
    return true;
}
