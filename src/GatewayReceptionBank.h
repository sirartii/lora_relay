#ifndef OPP_LORA_RELAY_GATEWAY_RECEPTION_BANK_H
#define OPP_LORA_RELAY_GATEWAY_RECEPTION_BANK_H

#include <cstdint>
#include <map>
#include <stdexcept>

namespace opp_lora_relay_official {

// Independent logical frequencies. Within each frequency preserve the
// simulator's 6 dB capture approximation, without claiming inter-SF isolation
// or a detailed concentrator demodulator/preamble-acquisition model.
// Time is represented in simulator ticks, so adjacent frames do not overlap.
class GatewayReceptionBank {
  public:
    enum class Result { Pending, Delivered, Sensitivity, Collision, TxBlocked, TxInterrupted };
    struct Signal {
        int channel;
        double power;
        std::int64_t end;
        Result result;
    };

    void arrive(long id, int channel, double power, bool aboveSensitivity,
                bool transmitting, std::int64_t now, std::int64_t end)
    {
        if (channel < 1 || channel > 8 || end <= now || signals.count(id))
            throw std::invalid_argument("Invalid gateway reception");
        Signal incoming{channel, power, end, !aboveSensitivity ? Result::Sensitivity :
            transmitting ? Result::TxBlocked : Result::Pending};
        if (!transmitting) {
            for (auto& entry : signals) {
                auto& other = entry.second;
                if (other.channel != channel || other.end <= now) continue;
                if (incoming.power <= other.power + 6 && incoming.result == Result::Pending)
                    incoming.result = Result::Collision;
                if (other.power <= incoming.power + 6 && other.result == Result::Pending)
                    other.result = Result::Collision;
            }
        }
        signals.emplace(id, incoming);
    }

    void startTransmission(std::int64_t now)
    {
        for (auto& entry : signals)
            if (entry.second.end > now && entry.second.result == Result::Pending)
                entry.second.result = Result::TxInterrupted;
    }

    Result complete(long id)
    {
        auto found = signals.find(id);
        if (found == signals.end()) throw std::invalid_argument("Unknown gateway reception");
        const auto result = found->second.result;
        signals.erase(found);
        return result == Result::Pending ? Result::Delivered : result;
    }

    bool decoding(std::int64_t now) const
    {
        for (const auto& entry : signals)
            if (entry.second.end > now && entry.second.result == Result::Pending) return true;
        return false;
    }

    std::size_t size() const { return signals.size(); }

    static const char *name(Result result)
    {
        switch (result) {
            case Result::Pending: return "pending";
            case Result::Delivered: return "delivered";
            case Result::Sensitivity: return "sensitivity";
            case Result::Collision: return "collision";
            case Result::TxBlocked: return "gw_tx_blocked";
            case Result::TxInterrupted: return "gw_tx_interrupted";
        }
        return "unknown";
    }

  private:
    // Keep failed signals until their physical end. They still interfere
    // with subsequent frames, including signals arriving during GW TX.
    std::map<long, Signal> signals;
};
} // namespace opp_lora_relay_official
#endif
