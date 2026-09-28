#include "WindowSchedule.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

using opp_lora_relay_official::sampleWindowSchedule;

int main()
{
    // Reject a duplicate and retain the entire original interval on every draw.
    std::vector<int> draws{80, 80, 10};
    std::size_t index = 0;
    auto deadlines = sampleWindowSchedule(1000, 1100, 2, [&](int width) {
        assert(width == 100);
        return draws.at(index++);
    });
    assert((deadlines == std::vector<long long>{1010,1080}) && index == 3);
    // More than two frames, endpoints and single-frame behavior.
    draws = {99,0,99,50,0,25}; index = 0;
    deadlines = sampleWindowSchedule(0,100,4,[&](int w) { assert(w==100); return draws.at(index++); });
    assert((deadlines == std::vector<long long>{0,25,50,99}));
    int calls = 0;
    deadlines = sampleWindowSchedule(1,10800000,1,[&](int w) {
        assert(w==10799999); ++calls; return 23;
    });
    assert(deadlines.front() == 24 && calls == 1);
    bool rejected = false;
    try { sampleWindowSchedule(0,1,2,[](int) { return 0; }); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { sampleWindowSchedule(0,10,1,[](int) { return 10; }); }
    catch (const std::out_of_range&) { rejected = true; }
    assert(rejected);
    // Uniform aggregate, but order-statistic means W/3 and 2W/3.
    std::mt19937 rng(417);
    const int width = 10800000, repeats = 200000;
    std::array<long long,12> bins{};
    double first = 0, second = 0;
    for (int i=0; i<repeats; ++i) {
        auto times = sampleWindowSchedule(0,width,2,[&](int w) {
            return std::uniform_int_distribution<int>(0,w-1)(rng);
        });
        assert(times[0] < times[1]);
        first += times[0]; second += times[1];
        for (auto t : times) ++bins[t/(width/12)];
    }
    first /= repeats*60000.; second /= repeats*60000.;
    assert(std::abs(first-60) < .5 && std::abs(second-120) < .5);
    const double expected = repeats*2./12;
    for (auto count : bins) assert(std::abs(count/expected-1) < .035);
    std::cout << "PASS: duplicate redraw, common interval, sorting, N=1/N>2, boundaries, invalid input, uniform histogram; means="
              << first << ", " << second << " min\n";
}
