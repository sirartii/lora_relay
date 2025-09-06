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

#include "Keeper.h"
#include <fstream>
#include <ctime>
#include <chrono>
#include <iomanip>

namespace opp_lora_relay_official {

Define_Module(Keeper);

void Keeper::reportUplinkCollision(long devIdx, long pktId)
{
    ulCollided_.insert(makeKey(devIdx, pktId));
}

void Keeper::reportDownlinkCollision(long devIdx, long pktId)
{
    dlCollided_.insert(makeKey(devIdx, pktId));
}

void Keeper::recordDownlinkReception(long devIdx, long pktId)
{
    dlDelivered_.insert(makeKey(devIdx, pktId));
}

void Keeper::initialize()
{
    noInitialized = 0;
    noInitializedEndnodes = 0;
    periodicStatsRecordInterval = 600;

    period = 0;
    //periodicStats = new PeriodicStats();
    std::memset((void*)&periodicStats, 0, sizeof(PeriodicStats));
    std::memset((void*)&globalStats, 0, sizeof(PeriodicStats));

    if (par("enableKeeperDistortion").intValue() != 0)
    {
        timerMessage = new cMessage("timerMessage");
        scheduleAt(intrand(100), timerMessage);
    }

    timerStats = new cMessage("timerStats");
    scheduleAt(simTime() + 3600, timerStats);           //one hour to make simulation stable, than stats are beeing recorded

    //endnodeStatusVector.setName("Devices online %");

    //hourly statistics
    packetsVector.setName("Packets Send");
    packetsDeliveredVector.setName("Packets Delivered");
    colisionsCountVector.setName("Collision Count");

    //endnodePerformance.setName("Node performance");
    //endnodePerformance.setMode(cHistogram::MODE_INTEGERS);
    //endnodePerformance.setRange(80, 130);
    //endnodePerformance.createUniformBins(-130,-40,1);
    //delaySignal = registerSignal("delaySignal");

    packteSent = 0;
    totalDuration = 0;
    offlinePercentage = 0;

    totalPacketTime = 0;
    totalPacketsDelivered = 0;
    totalPacketsTransmitted = 0;

    WATCH(offlinePercentage);
}

int Keeper::registerMe(std::string name, int index, DevType devType, Radio* radio)
{
   Node n;
   n.name = name;
   n.index = index;
   n.radioStatus = EndDeviceState::OFFLINE;
   n.devType = devType;
   n.radio = radio;

   radioDevices.push_back(n);
   EV<<name << "\n";

   distortionLevel = 0;
   noInitialized++;
   return noInitialized-1;
}

int Keeper::registerEndnode()
{
    noInitializedEndnodes++;
    return noInitializedEndnodes-1;
}

std::vector<Node> Keeper::returnRadioDevices()
{
   return radioDevices;
}

void Keeper::updateEndnodeState(int index, EndDeviceState status)
{
    if (index >= this->noInitialized)
        return;

    radioDevices[index].radioStatus = status;

    if (status == EndDeviceState::OFFLINE)
    {
        radioDevices[index].lastOnline = simTime();
    }
    if (status == EndDeviceState::ONLINE_DIRECT || status == EndDeviceState::ONLINE_RELAYED)
    {
        double OfflineTime = (simTime() - radioDevices[index].lastOnline).dbl();
        radioDevices[index].offlineMax = fmax(radioDevices[index].offlineMax, OfflineTime);
        radioDevices[index].offlineTotal += OfflineTime;
    }

    //updateStats();

}

void Keeper::recordFrameReception(int NodeFrom, int nodeTo, float RSSI, bool received)
{
    periodicStats.noPackets++;

    if (received)
        periodicStats.noPacketsDelivered++;


    if (RSSI > -40 && RSSI < -130)
        endnodePerformance.collect(RSSI);
    //emit(delaySignal, RSSI);
}

void Keeper::recordFrameTransmission(int NodeFrom, double msgDuration)
{
    packteSent++;
    totalDuration += msgDuration;
}

void Keeper::recordColision(int NodeFrom, int nodeTo)
{
    periodicStats.noCollisions++;
}

void Keeper::recordEnergyConsumption(int Node, double consumption)
{
    radioDevices[Node].energyConsumption = consumption;
}

void Keeper::recordOfflineTime(int Node, double OfflineTime)
{
    offlineTime.collect(OfflineTime);
}

void Keeper::updateStats()
{
    int noOfDevicesOnline = 0;
    //int noOfDevicesOffline = 0;
    int noOfDevices = 0;

    double offlineAll = 0;
    offlineMax = 0;

    for (size_t i = 0; i < radioDevices.size(); ++i) {
        if (radioDevices[i].devType == DevType::ENDNODE)
        {
            noOfDevices++;
            switch (radioDevices[i].radioStatus)
            {
                case EndDeviceState::OFFLINE:
                case EndDeviceState::JOIN_DIRECT:
                case EndDeviceState::JOIN_RELAY:
                    //noOfDevicesOffline++;
                    break;

                case EndDeviceState::ONLINE_DIRECT:
                case EndDeviceState::ONLINE_RELAYED:
                    noOfDevicesOnline++;
                    break;
            }
            offlineAll += radioDevices[i].offlineTotal;
            if (radioDevices[i].offlineTotal > offlineMax)
                offlineMax = radioDevices[i].offlineTotal;

            //radioDevices[i].offlineTotal

        }
    }
    offlinePercentage = (offlineAll / radioDevices.size()) / simTime().dbl();
    endnodeStatusVector.collect(noOfDevicesOnline*100/noOfDevices);

    //WATCH(offlinePercentage);

}

void Keeper::updatePeriodicalStats()
{
    packetsVector.record(periodicStats.noPackets);
    packetsDeliveredVector.record(periodicStats.noPacketsDelivered);
    colisionsCountVector.record(periodicStats.noCollisions);
    updateStats();

    globalStats.noCollisions += periodicStats.noCollisions;
    globalStats.noPackets += periodicStats.noPackets;
    globalStats.noPacketsDelivered += periodicStats.noPacketsDelivered;
    //globalStats.powerConsumption += periodicStats.powerConsumption;

    std::memset((void*)&periodicStats, 0, sizeof(PeriodicStats));
}

void Keeper::handleMessage(cMessage *msg)
{
    if (msg == timerStats)
    {
        updatePeriodicalStats();
        scheduleAfter(periodicStatsRecordInterval, msg);
    }

    if (msg == timerMessage)
    {
        int rand = intrand(1024);
        int level = 0;      //distortion

        if (rand <=128)
            level = 1;
        if (rand <=64)
            level = 2;
        if (rand <=32)
            level = 3;
        if (rand <= 16)
            level = 4;
        if (rand <= 8)
            level = 5;

        distortionLevel = level;
        this->getDisplayString().setTagArg("r",0,(level*50));

        scheduleAfter(2, timerMessage);         //schedule after 2 seconds
        period++;

    }
}

void Keeper::recordMessageTransmission(int NodeFrom, int PacketId)
{
    radioDevices[NodeFrom].LastPacketTransmissionTime = simTime();
    radioDevices[NodeFrom].lastPacketId = PacketId;
    totalPacketsTransmitted++;
}

void Keeper::recordMessageReception(long devIdx, long pktId)
{
    // Zliczamy czas doręczenia UL tylko raz dla (devIdx,pktId)
    const long long key = makeKey(devIdx, pktId);
    if (ulDelivered_.insert(key).second) {
       double deliveryTime = (simTime() - radioDevices[devIdx].LastPacketTransmissionTime).dbl();
       totalPacketTime += deliveryTime;
       // Nie zwiększamy tutaj totalPacketsDelivered – policzymy go jako UL∩DL w recordResultsToFile/finish
    }
}

// Pomocnicza różnica zbiorów: A \ B
static long diffCount(const std::unordered_set<long long>& A,
                      const std::unordered_set<long long>& B)
{
    long cnt = 0;
    for (const auto& k : A)
        if (B.find(k) == B.end())
            ++cnt;
    return cnt;
}


void Keeper::recordResultsToFile()
{
    std::ofstream outFile("exec_results.txt", std::ios::app);
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    const long noCollisionsUL = diffCount(ulCollided_, ulDelivered_);
    const long noCollisionsDL = diffCount(dlCollided_, dlDelivered_);
    const long noCollisionsSum = noCollisionsUL + noCollisionsDL;
    // Compute delivered from sets (strict end-to-end: UL delivered AND DL delivered)
    const long deliveredUL = static_cast<long>(ulDelivered_.size());
    const long deliveredDL = static_cast<long>(dlDelivered_.size());

    long deliveredEndToEnd = 0;
    if (deliveredUL > 0 && deliveredDL > 0) {
        // intersection count of keys present in both delivered sets
        if (deliveredUL <= deliveredDL) {
            for (const auto& k : ulDelivered_)
                if (dlDelivered_.find(k) != dlDelivered_.end())
                    ++deliveredEndToEnd;
        } else {
            for (const auto& k : dlDelivered_)
                if (ulDelivered_.find(k) != ulDelivered_.end())
                    ++deliveredEndToEnd;
        }
    } else {
        // Fallback: when only one side is tracked, take the non-zero one
        deliveredEndToEnd = std::max(deliveredUL, deliveredDL);
    }

    // Keep legacy field in sync so downstream code / scalars remain valid
    totalPacketsDelivered = static_cast<int>(deliveredEndToEnd);


    if (outFile.is_open()) {
        outFile << std::fixed << std::setprecision(2);
        outFile << std::ctime(&now);  // np. "Thu Jul 25 15:31:01 2025\n"
        outFile << "=== Simulation Parames ====\n";
        outFile << "ED = " << par("numEndnodes").str() << "\n";
        outFile << "Model = " << par("retransmissionModel").str() << "\n";
        outFile << "Distortion = " << par("enableKeeperDistortion").str() << "\n";
        outFile << "Max miss " << par("missedPacketsToSearchRelay").str() << "\n";

        outFile << "=== Results ===\n";
        outFile << "Total utilisation = " << 100*totalDuration / simTime() << "[%]\n";
        outFile << "Total packets transmitted = " << totalPacketsTransmitted << "\n";
        //outFile << "Total packets transmitted = " << packteSent << "\n";
        //outFile << "Total packets transmitted = " << globalStats.noPackets << "\n";
        outFile << "Total packets delivered = " << totalPacketsDelivered << "\n";
        outFile << "Delivered UL = " << deliveredUL << "\n";
        outFile << "Delivered DL = " << deliveredDL << "\n";
        //outFile << "Total packets delivered = " << globalStats.noPacketsDelivered << "\n";

        outFile << "Collisions uplink = " << noCollisionsUL << "\n";
        outFile << "Colissions downlink = " << noCollisionsDL << "\n";
        outFile << "Total collisions = " << noCollisionsSum /*globalStats.noCollisions*/ << "\n";

                outFile << "Packet delivery efficiency = " << (totalPacketsTransmitted > 0 ? 100.0 * (double)totalPacketsDelivered / (double)totalPacketsTransmitted : 0.0) << "[%]";
                outFile << "Average packet delivery time = " << (totalPacketsDelivered > 0 ? totalPacketTime / (double)totalPacketsDelivered : 0.0) << "";

        outFile << "Devices online max = " << endnodeStatusVector.getMax() << "\n";
        outFile << "Devices online avg = " << endnodeStatusVector.getMean() << "\n";
        outFile << "Devices online min = " << endnodeStatusVector.getMin() << "\n";

        outFile << "Devices offline count = " << offlineTime.getCount() << "\n";
        outFile << "Devices offline longest = " << offlineTime.getMax() << "[s]\n";
        outFile << "Devices offline avg = " << offlineTime.getMean() << "[s]\n";

        outFile << "Endnode consumption max = " << energyConsumption.getMax()/3600 << "[mAh]\n";
        outFile << "Endnode consumption avg = " << energyConsumption.getMean()/3600 << "[mAh]\n";
        outFile << "Endnode consumption min = " << energyConsumption.getMin()/3600 << "[mAh]\n";
        outFile << "Endnode consumption Relay = " << energyConsumptionRelay/3600 << "[mAh]\n";

        outFile << "--------------------------\n";

        outFile.close();
    } else {
        EV << "Unable to open results.txt for writing.\n";
    }
}

void Keeper::finish()

{
    for (size_t i = 0; i < radioDevices.size(); ++i) {
       if (radioDevices[i].devType == DevType::ENDNODE)
           energyConsumption.collect( radioDevices[i].energyConsumption );
       if (radioDevices[i].devType == DevType::RELAY)
           energyConsumptionRelay = radioDevices[i].energyConsumption;
    }

    // Derive end-to-end delivered (UL delivered & DL delivered)
    long deliveredUL = static_cast<long>(ulDelivered_.size());
    long deliveredDL = static_cast<long>(dlDelivered_.size());
    long deliveredEndToEnd = 0;
    if (deliveredUL > 0 && deliveredDL > 0) {
        if (deliveredUL <= deliveredDL) {
            for (const auto& k : ulDelivered_)
                if (dlDelivered_.find(k) != dlDelivered_.end())
                    ++deliveredEndToEnd;
        } else {
            for (const auto& k : dlDelivered_)
                if (ulDelivered_.find(k) != ulDelivered_.end())
                    ++deliveredEndToEnd;
        }
    } else {
        deliveredEndToEnd = std::max(deliveredUL, deliveredDL);
    }
    totalPacketsDelivered = static_cast<int>(deliveredEndToEnd);

    recordScalar("Total utilisation", totalDuration / simTime());
    recordScalar("Average packet time", (totalPacketsDelivered > 0 ? totalPacketTime / totalPacketsDelivered : 0.0));
    recordScalar("Total packets delivered", totalPacketsDelivered);

    recordResultsToFile();
}


} //namespace
