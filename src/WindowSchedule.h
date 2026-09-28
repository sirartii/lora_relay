#ifndef OPP_LORA_RELAY_WINDOW_SCHEDULE_H
#define OPP_LORA_RELAY_WINDOW_SCHEDULE_H

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace opp_lora_relay_official {

// Sample distinct application deadlines uniformly from the SAME millisecond
// interval, then order them. Duplicate deadlines of this ED are redrawn; this
// does not avoid radio collisions with other EDs or impose an airtime guard.
template <typename DrawOffset>
std::vector<long long> sampleWindowSchedule(long long firstMs, long long endMs,
                                          int count, DrawOffset drawOffset)
{
    const long long width = endMs - firstMs;
    if (count < 1 || width < count || width > std::numeric_limits<int>::max())
        throw std::invalid_argument("Window must contain enough distinct millisecond deadlines");
    std::vector<long long> deadlines;
    std::unordered_set<long long> used;
    deadlines.reserve(count);
    while (static_cast<int>(deadlines.size()) < count) {
        const long long offset = drawOffset(static_cast<int>(width));
        if (offset < 0 || offset >= width)
            throw std::out_of_range("Window deadline draw outside sampling interval");
        const long long deadline = firstMs + offset;
        if (used.insert(deadline).second)
            deadlines.push_back(deadline);
    }
    std::sort(deadlines.begin(), deadlines.end());
    return deadlines;
}

} // namespace opp_lora_relay_official

#endif
