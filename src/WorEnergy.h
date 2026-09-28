#ifndef OPP_LORA_RELAY_WOR_ENERGY_H
#define OPP_LORA_RELAY_WOR_ENERGY_H
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace opp_lora_relay_official {
// Reference SX126x CAD profile at BW=125 kHz, matching LoRaPacket::symbolTime.
// Measured durations: Semtech AN1200.48 rev 2.1, Tables 93, 95-99.
// cadSymbols is hardware cadSymbolNum, not TS011 CadToRx or ChannelScan.
inline double sx126xCadDurationSeconds(int sf, int cadSymbols)
{
    const int symbols[] = {1, 2, 4, 8, 16};
    const double milliseconds[6][5] = {
        {1.6439, 2.6641, 4.7155, 8.8088, 17.01},
        {3.3384, 5.3872, 9.4806, 17.676, 34.057},
        {6.8625, 10.955, 19.145, 35.531, 68.29},
        {14.156, 22.345, 38.732, 71.496, 137.03},
        {29.26, 45.64, 78.409, 143.95, 275.02},
        {60.493, 93.256, 158.79, 289.87, 552.01}
    };
    if (sf < 7 || sf > 12)
        throw std::invalid_argument("SX126x CAD energy profile requires SF7..SF12 at BW125 kHz");
    for (int i = 0; i < 5; ++i)
        if (cadSymbols == symbols[i]) return milliseconds[sf - 7][i] / 1000.0;
    throw std::invalid_argument("CAD symbol count must be 1, 2, 4, 8 or 16");
}
inline double worStandbyCurrentMa(double period, double cadDuration,
                                 int channels, double cadCurrent, double sleepCurrent)
{
    if (!std::isfinite(period) || !std::isfinite(cadDuration) ||
        !std::isfinite(cadCurrent) || !std::isfinite(sleepCurrent) ||
        period <= 0 || cadDuration <= 0 || (channels != 1 && channels != 2) ||
        sleepCurrent < 0 || cadCurrent < sleepCurrent)
        throw std::invalid_argument("Invalid CAD energy parameters");
    const double duty = channels * cadDuration / period;
    if (duty > 1.0 + 1e-12)
        throw std::invalid_argument("CAD scans overlap: single RF chain requires T_CAD >= channels * CAD duration");
    return sleepCurrent + (cadCurrent - sleepCurrent) * std::min(1.0, duty);
}
} // namespace opp_lora_relay_official
#endif
