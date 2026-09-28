    //
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef __OPP_LORA_RELAY_OFFICIAL_LNS_H_
#define __OPP_LORA_RELAY_OFFICIAL_LNS_H_

#include <omnetpp.h>
#include "Keeper.h"
#include <unordered_map>
#include <vector>
#include <limits>
#include <string>
#include <set>
#include "LoRaPacket.h"

using namespace omnetpp;

namespace opp_lora_relay_official {

/**
 * TODO - Generated class
 */
class LNS : public omnetpp::cSimpleModule
{
  protected:
    Keeper* keeper = nullptr;
    long packetCounter = 0;

    // --- Konfiguracja ---
    omnetpp::simtime_t dedupHoldoff = 0.1; // fallback; nadpiszemy z par, jeśli istnieje

    // --- Średnie RSSI per (urządzenie, gate) ---
    struct RssiAvg {
        double sum = 0.0; long n = 0;
        void add(double v) { sum += v; ++n; }
        double mean() const { return n ? (sum / n) : -std::numeric_limits<double>::infinity(); }
    };
    // Klucz urządzenia = KeeperIndex (long)
    std::unordered_map<long, std::unordered_map<int, RssiAvg>> avgRssiByDevGate;

    // --- Rejestr duplikatów ---
    struct Rec { int gateIdx; double rssi; };
    struct DedupEntry {
        long devIdx = -1;       // KeeperIndex
        long pktId  = -1;       // PacketId
        omnetpp::simtime_t t0;  // czas pierwszego nadejścia
        omnetpp::cMessage* timer = nullptr; // self-message kończący okno dedup
        LoRaPacket* representative = nullptr; // jeden egzemplarz UL do przetworzenia
        std::vector<Rec> receptions;          // lista kopii: (wejście, RSSI)
    };
    struct ReceivedFrame {
        double x;             // senderPositionX
        double y;             // senderPositionY
        short  sf;            // spreadingFactor
        double rssi;          // LoRaPacket::rssi
        double rssi_margin;   // LoRaPacket::rssi_margin
    };
    // 64-bitowy klucz: (devIdx << 32) ^ pktId
    static inline long long makeKey(long devIdx, long pktId) {
        return ( (static_cast<long long>(devIdx) << 32) ^ (static_cast<unsigned long long>(pktId) & 0xFFFFFFFFull) );
    }
    std::map<std::string, DedupEntry> dedup;
    std::set<long long> deliveredApplicationFrames;
    std::vector<ReceivedFrame> receivedFrames;
    double relayForwardDelay = 1.0;
    omnetpp::cStdDev networkTimeCorrectionError;

    enum { KIND_DEDUP_TIMER = 0x0DB1 };

    // --- Pomocnicze ---
    void onUplinkArrived(LoRaPacket* ul, int inGateIdx);
    void onDedupTimer(omnetpp::cMessage* self);
    void processUniqueUplink(DedupEntry& e, int preferOutGate);
    void calcADR(LoRaPacket* packet, double rssiMargin);

    // --- cSimpleModule ---
    virtual void initialize() override;
    virtual void handleMessage(omnetpp::cMessage *msg) override;
    virtual void finish() override;
};
} // namespace

#endif
