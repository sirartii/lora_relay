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
#include "Radio.h"
#include <fstream>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>   // std::atof
#include <ctime>     // std::time, std::ctime

namespace opp_lora_relay_official {

Define_Module(Keeper);

void Keeper::reportUplinkCollision(long devIdx, long pktId)
{
    const uint64_t key = makeKey(devIdx, pktId);
    if (isCollectingStatistics() && ulTransmitted_.find(key) != ulTransmitted_.end())
        ulCollided_.insert(key);
}

void Keeper::reportDownlinkCollision(long devIdx, long pktId)
{
    if (isCollectingStatistics())
        dlCollided_.insert(makeKey(devIdx, pktId));
}

void Keeper::recordDownlinkReception(long devIdx, long pktId)
{
    if (!isCollectingStatistics())
        return;

    const uint64_t key = makeKey(devIdx, pktId);
    if (dlDelivered_.insert(key).second) {
        auto it = ulDeliveredDay_.find(key);
        if (it != ulDeliveredDay_.end() && dataUlTransmitted_.count(key)) {
            // Dzień liczymy wg UL
            dailyReadDeviceDayE2E_.insert(makeDevDayKey(devIdx, it->second));
        }
    }
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

const std::vector<Node>& Keeper::returnRadioDevices() const
{
   return radioDevices;
}

bool Keeper::hasRelayPath() const
{
    for (const Node& node : radioDevices) {
        if (node.devType == DevType::RELAY || node.devType == DevType::IPBRIDGE)
            return true;
    }
    return false;
}

bool Keeper::isCollectingStatistics() const
{
    return simTime() >= getSimulation()->getWarmupPeriod();
}

bool Keeper::epRelayEligible(int device) const
{
    if (!par("epDesignationRestricted").boolValue()) return true;
    const auto &node = radioDevices.at(device);
    if (node.devType != DevType::ENDNODE || node.radio == nullptr) return false;
    const std::string name = node.radio->getParentModule()->getName();
    return name == "endnode1" || name == "endnode2";
}

bool Keeper::ts011RelayEligible(int device) const
{
    if (!par("ts011WhitelistRestricted").boolValue()) return true;
    const auto &node = radioDevices.at(device);
    if (node.devType != DevType::ENDNODE || node.radio == nullptr) return false;
    const std::string name = node.radio->getParentModule()->getName();
    return name == "endnode1" || name == "endnode2";
}

void Keeper::recordEpRecovery(int device, int packet, int event)
{
    if (par("exportFrameRecords").boolValue())
        epRecoveryEvents_.push_back({device, packet, event, simTime().dbl()});
}

int Keeper::firstStatisticsDay() const
{
    const double warmupSeconds = getSimulation()->getWarmupPeriod().dbl();
    return std::max(0, (int)std::ceil(warmupSeconds / SECONDS_PER_DAY));
}

void Keeper::updateEndnodeState(int index, EndDeviceState status)
{
    if (index >= this->noInitialized)
        return;

    EndDeviceState oldStatus = radioDevices[index].radioStatus;
    radioDevices[index].radioStatus = status;

    if (status == EndDeviceState::OFFLINE)
    {
        radioDevices[index].lastOnline = simTime();
        if (isCollectingStatistics() &&
            (oldStatus == EndDeviceState::ONLINE_DIRECT || oldStatus == EndDeviceState::ONLINE_RELAYED))
            radioDevices[index].offlineCount++;
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
    if (!isCollectingStatistics())
        return;

    periodicStats.noPackets++;

    if (received)
        periodicStats.noPacketsDelivered++;


    if (RSSI > -40 && RSSI < -130)
        endnodePerformance.collect(RSSI);
    //emit(delaySignal, RSSI);
}

void Keeper::recordFrameTransmission(int NodeFrom, double msgDuration)
{
    if (!isCollectingStatistics())
        return;

    packteSent++;
    totalDuration += msgDuration;
}

void Keeper::recordColision(int NodeFrom, int nodeTo)
{
    if (isCollectingStatistics())
        periodicStats.noCollisions++;
}

void Keeper::recordEnergyConsumption(int Node, double consumption)
{
    radioDevices[Node].energyConsumption = consumption;
}

void Keeper::recordOfflineTime(int Node, double OfflineTime)
{
    if (isCollectingStatistics())
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

void Keeper::recordMessageTransmission(int NodeFrom, int PacketId, bool isUplink)
{
    if (isUplink)
    {
        radioDevices[NodeFrom].LastPacketTransmissionTime = simTime();
        radioDevices[NodeFrom].lastPacketId = PacketId;
    }

    if (!isCollectingStatistics())
        return;

    if (isUplink) {
        const uint64_t key = makeKey(NodeFrom, PacketId);
        if (ulTransmitted_.insert(key).second) {
            ulFirstTransmissionTime_[key] = simTime();
            totalPacketsTransmittedUL++;
            totalPacketsTransmitted++;
        }
    }
    else {
        totalPacketsTransmittedDL++;
        totalPacketsTransmitted++;
    }
}

void Keeper::recordFrameDetails(int device, int packet, bool join, bool ack, bool relayed, simtime_t applicationTime)
{
    if (!isCollectingStatistics()) return;
    if (!join) dataUlTransmitted_.insert(makeKey(device, packet));
    if (!par("exportFrameRecords").boolValue()) return;
    auto &item = frameDetails_[makeKey(device, packet)];
    item.join = join;
    item.ack = ack;
    item.applicationTime = applicationTime;
    ++item.attempts;
    if (relayed) ++item.relayAttempts;
}

void Keeper::recordActualRelayUplink(int device, int packet, int relay)
{
    if (!isCollectingStatistics() || !par("exportFrameRecords").boolValue()) return;
    auto it = frameDetails_.find(makeKey(device, packet));
    if (it == frameDetails_.end()) throw cRuntimeError("Relay TX without ED frame record");
    ++it->second.actualRelayTx;
    it->second.actualRelayIndex = radioDevices.at(relay).index;
    actualRelayEvents_.push_back({device, packet, radioDevices.at(relay).index, simTime().dbl()});
}

void Keeper::recordMessageReception(long devIdx, long pktId)
{
    // Zliczamy czas doręczenia UL tylko raz dla (devIdx,pktId)
    const uint64_t key = makeKey(devIdx, pktId);
    if (!isCollectingStatistics() || ulTransmitted_.find(key) == ulTransmitted_.end())
        return;

    if (ulDelivered_.insert(key).second)
    {
        auto detail = frameDetails_.find(key);
        if (detail != frameDetails_.end()) detail->second.receptionTime = simTime();
        const auto firstTx = ulFirstTransmissionTime_.find(key);
        if (firstTx != ulFirstTransmissionTime_.end())
            totalPacketTime += std::max(0.0, (simTime() - firstTx->second).dbl());
        const int d = dayIndex(simTime());
        ulDeliveredDay_[key] = d;

        // An application read requires DATA. Join only establishes a session.
        // Uplink read success does not require a downlink acknowledgment.
        if (dataUlTransmitted_.count(key)) {
            dailyReadDeviceDayUL_.insert(makeDevDayKey(devIdx, d));
            dailyDeviceDaySuccess_.insert(makeDevDayKey(devIdx, d));
            if (dlDelivered_.count(key))
                dailyReadDeviceDayE2E_.insert(makeDevDayKey(devIdx, d));
        }
    }
}

// Pomocnicza różnica zbiorów: A \ B
static long diffCount(const std::unordered_set<uint64_t>& A,
                      const std::unordered_set<uint64_t>& B)
{
    long cnt = 0;
    for (const auto& k : A)
        if (B.find(k) == B.end())
            ++cnt;
    return cnt;
}

inline void getXYFromModule(cModule* m, double& x, double& y) {
    x = y = 0.0;
    if (!m) return;
    const char* xs = m->getDisplayString().getTagArg("p", 0);
    const char* ys = m->getDisplayString().getTagArg("p", 1);
    if (m->hasPar("xpos")) x = m->par("xpos").doubleValue();
    else                   x = (xs && *xs) ? std::atof(xs) : 0.0;
    if (m->hasPar("ypos")) y = m->par("ypos").doubleValue();
    else                   y = (ys && *ys) ? std::atof(ys) : 0.0;
}

inline bool fileExists(const char* path) {
    std::ifstream f(path); return f.good();
}

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

struct Anchor { const char* role; int idx; double x; double y; };

void collectAnchorsRecursive(cModule* parent, std::vector<Anchor>& out) {
    for (cModule::SubmoduleIterator it(parent); !it.end(); ++it) {
        cModule* m = *it;
        std::string n  = toLower(m->getName());
        std::string fn = toLower(m->getFullName());

        const char* role = nullptr;
        if (n.find("gateway") != std::string::npos || fn.find("gateway") != std::string::npos)
            role = "GW";
        else if (n.find("relay") != std::string::npos || fn.find("relay") != std::string::npos)
            role = "RELAY";
        else if (n.find("ipbridge") != std::string::npos || fn.find("ipbridge") != std::string::npos ||
                 n == "ipb" || fn.find("ipb") != std::string::npos)
            role = "IPB";

        if (role) {
            double x=0.0, y=0.0; getXYFromModule(m, x, y);
            const int idx = m->isVector() ? m->getIndex() : 0;
            out.push_back({role, idx, x, y});
        }
        // rekurencja wgłąb
        collectAnchorsRecursive(m, out);
    }
}

void writeAnchorsIfMissing(const char* filename) {
    if (fileExists(filename)) return;

    std::vector<Anchor> anchors;
    if (cModule* root = getSimulation()->getSystemModule())
        collectAnchorsRecursive(root, anchors);

    std::ofstream topo(filename, std::ios::out);
    topo << std::fixed << std::setprecision(2);
    std::time_t now = std::time(nullptr);

    topo << "# Fixed nodes (Gateways/Relays/IPBridge)\n";
    topo << "# Written on: " << std::ctime(&now) << "\n";
    topo << "role,idx,x,y\n";
    for (const auto& a : anchors)
        topo << a.role << "," << a.idx << "," << a.x << "," << a.y << "\n";
    topo << "\n";
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

    // Daily read success
    int endnodes = countEndnodes();
    int simDays  = std::max(1, (int)std::ceil(simTime().dbl() / SECONDS_PER_DAY));
    const int warmupDays = firstStatisticsDay();
    const int statisticsDays = std::max(0, simDays - warmupDays);
    long long totalDeviceDays = 1LL * endnodes * statisticsDays;

    const long long successfulUL = (long long)dailyReadDeviceDayUL_.size();
    const double dailyUL = (totalDeviceDays > 0)
        ? 100.0 * (double)successfulUL / (double)totalDeviceDays : 0.0;

    const long long successfulE2E = (long long)dailyReadDeviceDayE2E_.size();
    const double dailyE2E = (totalDeviceDays > 0)
        ? 100.0 * (double)successfulE2E / (double)totalDeviceDays : 0.0;


    // --- czas aktywnych okien na potrzeby wykorzystania kanału
    cModule* sys = getSimulation()->getSystemModule();
    const double win1_min = sys->hasPar("transmittingWindowDuration1")? static_cast<double>(sys->par("transmittingWindowDuration1").intValue()) : 0.0;
    const double win2_min = sys->hasPar("transmittingWindowDuration2")? static_cast<double>(sys->par("transmittingWindowDuration2").intValue()) : 0.0;

    // sekundy aktywne (na dobę i łącznie w symulacji)
    const double activeSecsPerDay  = (std::max(0.0, win1_min) + std::max(0.0, win2_min)) * 60.0;
    const double activeSecsTotal   = activeSecsPerDay * statisticsDays;

    // wykorzystanie względem okien (0..1) i dla porównania stare (0..1)
    const double utilPerWindow = (activeSecsTotal > 0.0) ? (totalDuration / activeSecsTotal) : 0.0;
    // Keep legacy field in sync so downstream code / scalars remain valid
    totalPacketsDelivered = static_cast<int>(deliveredEndToEnd);
    const int simDays_local  = std::max(1, (int)std::ceil(simTime().dbl() / SECONDS_PER_DAY));

    totalPacketsTransmitted = totalPacketsTransmittedUL + totalPacketsTransmittedDL;

    int rejoinCount = 0;
    for (size_t i = 0; i < radioDevices.size(); ++i) {
        rejoinCount += radioDevices[i].offlineCount;
    }


    if (outFile.is_open()) {
        outFile << std::fixed << std::setprecision(2);
        outFile << std::ctime(&now);  // np. "Thu Jul 25 15:31:01 2025\n"
        outFile <<  "\n\n";
        outFile << "============== Simulation Parames ================\n";
        outFile << "ED             = " << endnodes << "\n";
        outFile << "Scenario       = " << getSimulation()->getSystemModule()->getModuleType()->getName() << "\n";
        outFile << "Model          = " << par("retransmissionModel").str() << "\n";
        outFile << "Distortion     = " << par("enableKeeperDistortion").str() << "\n";
        outFile << "Max miss       = " << par("missedPacketsToSearchRelay").str() << "\n";
        outFile << "Simulated days = " << simDays << "\n";
        outFile << "Warmup days    = " << warmupDays << "\n";
        outFile << "Statistics days= " << statisticsDays << "\n";
        outFile << "Window len [h] = " << getSimulation()->getSystemModule()->par("transmittingWindowDuration1").str() << "\n";
        outFile << "Frames / window= " << getSimulation()->getSystemModule()->par("framesPerWindowMin").str() << "\n";
        outFile <<  "\n";

        outFile << "============== Results ================\n";
        outFile << "Total utilisation = " << (utilPerWindow * 100.0) << "[%]\n";            //liczone tylko dla okien zgłoszeniowych
        //outFile << "Total utilisation (per day)     = " << (utilPerDay    * 100.0) << "[%]\n";
        outFile <<  "\n";
        //outFile << "Daily read base: endnodes=" << endnodes << " days=" << simDays << " totalDeviceDays=" << totalDeviceDays << "\n";

        outFile << "Packets transmitted UL = " << totalPacketsTransmittedUL << "\n";
        outFile << "Packets transmitted DL = " << totalPacketsTransmittedDL << "\n";
        outFile << "Packets transmitted = " << totalPacketsTransmitted << "\n";
        outFile <<  "\n";

        outFile << "Packets delivered UL = " << deliveredUL << "\n";
        outFile << "Packets delivered DL = " << deliveredDL << "\n";
        outFile << "Packets delivered = " << deliveredUL+deliveredDL << "\n";
        //outFile << "Packets delivered* = " << totalPacketsDelivered << "\n";
        outFile <<  "\n";

        outFile << "Packets delivered UL ef = " << (totalPacketsTransmittedUL > 0 ? 100.0 * (double)deliveredUL / (double)totalPacketsTransmittedUL : 0.0) << "[%]\n";
        outFile << "Packets delivered DL ef = " << (totalPacketsTransmittedDL > 0 ? 100.0 * (double)deliveredDL / (double)totalPacketsTransmittedDL : 0.0) << "[%]\n";
        outFile << "Average packet delivery time = " << (deliveredUL > 0 ? totalPacketTime / (double)deliveredUL : 0.0) << "\n";
        outFile <<  "\n";

        //outFile << "Total packets delivered = " << globalStats.noPacketsDelivered << "\n";
        //outFile << "Daily read success = " << dailyReadSuccess << "[%]\n";
        //outFile << "Daily device-days successful = " << successfulDeviceDays << " / " << totalDeviceDays << "\n";

        //outFile << "Daily read success (E2E UL+DL) = " << dailyE2E << "[%]\n";
        //outFile << "Daily device-days successful (E2E) = "
        //        << successfulE2E << " / " << totalDeviceDays << "\n";

        outFile << "Days device-days total (UL)[%] = " << totalDeviceDays << "\n";
        outFile << "Days device-days successful (UL)[%] = " << successfulUL << "\n";
        outFile << "Daily read success (UL)[%] = " << dailyUL << "[%]\n";
        outFile << "Daily read success (UL/DL)[%] = " << dailyE2E << "[%]\n";
        outFile << "Daily read definition = application DATA only; Join excluded; UL does not require ACK\n";
        outFile << "Packets transmitted DATA UL = " << dataUlTransmitted_.size() << "\n";
        long deliveredData = 0;
        for (auto key : dataUlTransmitted_) deliveredData += ulDelivered_.count(key);
        outFile << "Packets delivered DATA UL = " << deliveredData << "\n";
        outFile <<  "\n";

        outFile << "Collisions uplink = " << noCollisionsUL << "\n";
        outFile << "Colissions downlink = " << noCollisionsDL << "\n";
        outFile << "Total collisions = " << noCollisionsSum /*globalStats.noCollisions*/ << "\n";
        outFile <<  "\n";


        //outFile << "Devices online max = " << endnodeStatusVector.getMax() << "\n";
        //outFile << "Devices online avg = " << endnodeStatusVector.getMean() << "\n";
        //outFile << "Devices online min = " << endnodeStatusVector.getMin() << "\n";

        outFile << "Devices re-join count = " << rejoinCount << "\n";           //-endnodes ze względu na 1 laczenie
        outFile << "Devices offline longest = " << offlineTime.getMax() << "[s]\n";
        outFile << "Devices offline avg = " << offlineTime.getMean() << "[s]\n";
        outFile <<  "\n";

        outFile << "Endnode offset current max [uAh] = " << 1000*energyConsumption.getMax()/(simDays_local*24) << "\n";
        outFile << "Endnode offset current min [uAh] = " << 1000*energyConsumption.getMin()/(simDays_local*24) << "\n";
        outFile << "Endnode offset current Relay [uAh] = " << 1000*energyConsumptionRelay/(simDays_local*24) << "\n";

        outFile << "--------------------------\n";

        outFile.close();
    } else {
        EV << "Unable to open results.txt for writing.\n";
    }


    writeAnchorsIfMissing("exec_results_topology.txt");
    std::ofstream topoFile("exec_results_topology.txt", std::ios::app);
    if (topoFile.is_open()) {
        topoFile << std::fixed << std::setprecision(2);
        topoFile << std::ctime(&now);
        topoFile << "\n";

        const int endnodes_local = countEndnodes();

        topoFile << "ED             = " << endnodes_local << "\n";
        topoFile << "Simulated days = " << simDays  << "\n";
        topoFile << "idx,x,y,avg_daily_success_UL[%]\n";

        for (size_t i = 0; i < radioDevices.size(); ++i) {
            if (radioDevices[i].devType != DevType::ENDNODE)
                continue;

            const int devIdx = static_cast<int>(i);

            int successDaysUL = 0;
            for (int d = warmupDays; d < simDays; ++d) {
                const uint64_t key = makeDevDayKey(devIdx, d);
                if (dailyReadDeviceDayUL_.find(key) != dailyReadDeviceDayUL_.end())
                    ++successDaysUL;
            }
            const double avgUL = (statisticsDays > 0)
                ? 100.0 * (double)successDaysUL / (double)statisticsDays : 0.0;

            double x = 0.0, y = 0.0;
            if (radioDevices[i].radio) {
                cModule* nodeMod = ((cModule*)(radioDevices[i].radio))->getParentModule(); // Endnode
                const char* xs = nodeMod->getDisplayString().getTagArg("p", 0);
                const char* ys = nodeMod->getDisplayString().getTagArg("p", 1);

                if (nodeMod->hasPar("xpos")) x = nodeMod->par("xpos").doubleValue();
                else                         x = (xs && *xs) ? atof(xs) : 0.0;

                if (nodeMod->hasPar("ypos")) y = nodeMod->par("ypos").doubleValue();
                else                         y = (ys && *ys) ? atof(ys) : 0.0;
            }
            // zapisujemy indeks ED (Node.index), współrzędne i średnie UL
            topoFile << radioDevices[i].index << "," << x << "," << y << "," << avgUL << "\n";
        }
        topoFile << "\n\n";
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

    // Also export scalars for plotting
    int endnodes = countEndnodes();
    int simDays  = std::max(1, (int)std::ceil(simTime().dbl() / SECONDS_PER_DAY));
    long long totalDeviceDays      = 1LL * endnodes * std::max(0, simDays - firstStatisticsDay());
    long long successfulDeviceDays = (long long)dailyDeviceDaySuccess_.size();
    double dailyReadSuccess = (totalDeviceDays > 0)
                              ? (100.0 * (double)successfulDeviceDays / (double)totalDeviceDays)
                              : 0.0;

    recordScalar("Total utilisation", totalDuration / simTime());
    recordScalar("Average packet time", (deliveredUL > 0 ? totalPacketTime / deliveredUL : 0.0));
    recordScalar("Total packets delivered", totalPacketsDelivered);
    recordScalar("Daily succes rate", dailyReadSuccess);
    recordScalar("Daily read success DATA percent", dailyReadSuccess);
    recordScalar("Successful DATA device-days", successfulDeviceDays);

    recordResultsToFile();
    if (par("exportFrameRecords").boolValue()) {
        std::ofstream frames("n07_frames.json", std::ios::trunc);
        if (!frames) throw cRuntimeError("Cannot write per-frame records");
        frames << "{\"schema\":3,\"columns\":[\"device\",\"packet_id\",\"group\",\"tx_seconds\",\"delivered\",\"rx_day_zero_based\",\"join\",\"ack_required\",\"application_seconds\",\"rx_seconds\",\"attempts\",\"relay_attempts\",\"actual_relay_tx\",\"actual_relay_index\"],\"frames\":[";
        frames << std::setprecision(17);
        bool first = true;
        for (const auto &item : ulFirstTransmissionTime_) {
            const size_t device = item.first >> 32;
            const auto &detail = frameDetails_.at(item.first);
            const std::string name = radioDevices.at(device).radio->getParentModule()->getName();
            const int group = name == "endnode" ? 0 : (name == "endnode1" ? 1 : 2);
            const bool delivered = ulDelivered_.count(item.first) != 0;
            if (!first) frames << ",";
            first = false;
            frames << "[" << device << "," << static_cast<uint32_t>(item.first) << "," << group << ","
                   << item.second.dbl() << "," << delivered << "," << (delivered ? ulDeliveredDay_.at(item.first) : -1)
                   << "," << detail.join << "," << detail.ack << "," << detail.applicationTime.dbl()
                   << "," << detail.receptionTime.dbl() << "," << detail.attempts << "," << detail.relayAttempts
                   << "," << detail.actualRelayTx << "," << detail.actualRelayIndex << "]";
        }
        frames << "]}\n";
        // Actual RF start times: attribute forwarding to the day it occurred,
        // not to the possibly earlier application-generation day.
        std::ofstream events("relay_events.json", std::ios::trunc);
        if (!events) throw cRuntimeError("Cannot write relay diagnostics");
        events << std::setprecision(17)
               << "{\"schema\":1,\"relay_columns\":[\"device\",\"packet\",\"relay\",\"tx_seconds\"],\"relay_tx\":[";
        first = true;
        for (const auto &e : actualRelayEvents_) {
            if (!first) events << ",";
            first = false;
            events << "[" << e.device << "," << e.packet << "," << e.code << "," << e.time << "]";
        }
        events << "],\"recovery_columns\":[\"device\",\"packet\",\"event\",\"seconds\"],\"recovery_event_codes\":{\"1\":\"lost_session_start_relay_join\",\"2\":\"failed_relay_join_return_direct\"},\"ep_recovery\":[";
        first = true;
        for (const auto &e : epRecoveryEvents_) {
            if (!first) events << ",";
            first = false;
            events << "[" << e.device << "," << e.packet << "," << e.code << "," << e.time << "]";
        }
        events << "]}\n";
    }
}

int Keeper::countEndnodes() const {
    int n = 0;
    for (const auto& nd : radioDevices)
        if (nd.devType == DevType::ENDNODE) ++n;
    return n;
}

} //namespace
