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

#ifndef __OPP_LORA_RELAY_OFFICIAL_KEEPER_H_
#define __OPP_LORA_RELAY_OFFICIAL_KEEPER_H_

#include <omnetpp.h>
#include <vector>
#include "common.h"

using namespace omnetpp;

namespace opp_lora_relay_official {

class Radio;

struct Node
{
    std::string name;
    int index;
    EndDeviceState radioStatus;
    DevType devType;
    int lastPacketId;
    simtime_t LastPacketTransmissionTime;

    simtime_t lastOnline;
    double offlineMax;
    double offlineTotal;
    double offlinePercentage;
    double energyConsumption;
    Radio *radio = nullptr;
};

struct PeriodicStats{
    int noPackets;
    int noCollisions;
    int noPacketsDelivered;
    //float powerConsumption;
};
/**
 * TODO - Generated class
 */
class Keeper : public cSimpleModule
{
public:
    int registerMe(std::string name, int index, DevType devTyp);
    int registerMe(std::string name, int index, DevType devTyp, Radio* radio);
    int registerEndnode();

    std::vector<Node> returnRadioDevices();
    void updateEndnodeState(int index, EndDeviceState status);
    void recordFrameTransmission(int NodeFrom, double msgDuration);
    void recordFrameReception(int NodeFrom, int nodeTo, float RSSI, bool reveived);
    void recordColision(int NodeFrom, int nodeTo);
    void recordEnergyConsumption(int Node, double consumption);

    void recordMessageTransmission(int NodeFrom, int MsgNo);
    //void recordMessageReception(int NodeFrom, int MsgNo);
    void recordResultsToFile();
    void recordOfflineTime(int Node, double OfflineTime);

    // ===== NOWE API =====
    void recordMessageReception(long devIdx, long pktId);
    void reportUplinkCollision(long devIdx, long pktId);     // Radio w GW/Bridge gdy UL padł przez kolizję
    void reportDownlinkCollision(long devIdx, long pktId);   // Radio w EndDevice gdy DL padł przez kolizję
    void recordDownlinkReception(long devIdx, long pktId);   // EndDevice gdy DL dotarł


    cMessage *timerMessage;
    cMessage *timerStats;

    int distortionLevel;
    int periodicStatsRecordInterval;

    cStdDev endnodeStatusVector;
    cOutVector packetsVector;
    cOutVector packetsDeliveredVector;
    cOutVector colisionsCountVector;


    //cHistogram endnodePerformance;
    cStdDev offlineTime;
    cStdDev endnodePerformance;
    cStdDev energyConsumption;
    double energyConsumptionRelay;



    //simsignal_t delaySignal;

    PeriodicStats periodicStats;
    PeriodicStats globalStats;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    std::vector<Node> radioDevices;



    // ===== NOWE: magazyny do rozliczenia 'kolizje -> niedostarczenie' =====
    // Klucz 64-bit: (devIdx << 32) ^ pktId
    static inline long long makeKey(long devIdx, long pktId) {
        return ( (static_cast<long long>(devIdx) << 32)
               ^ (static_cast<unsigned long long>(pktId) & 0xFFFFFFFFull) );
    }
    // UL
    std::unordered_set<long long> ulCollided_;   // było co najmniej jedno odrzucenie UL z powodu kolizji
    std::unordered_set<long long> ulDelivered_;  // LNS odebrał UL (recordMessageReception)
    // DL
    std::unordered_set<long long> dlCollided_;   // EndDevice odrzucił DL z powodu kolizji
    std::unordered_set<long long> dlDelivered_;  // EndDevice odebrał DL (ACK/JOIN ACCEPT)
    // (opcjonalnie) zachowaj stary licznik dla kompatybilności; przestanie być używany:
    long noCollisions = 0; // DEPRECATED – nie używaj w nowych miejscach

  private:
    void updateStats();
    void updatePeriodicalStats();
    int noInitialized;
    int noInitializedEndnodes;
    int period;
    int packteSent;
    double totalDuration;

    double totalPacketTime;
    int totalPacketsDelivered;
    int totalPacketsTransmitted;
    double offlinePercentage;
    double offlineMax;
};

} //namespace

#endif
