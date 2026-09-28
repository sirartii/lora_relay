#include "LNS.h"

#include <algorithm>
#include <fstream>
#include <iomanip>

#include "common.h"

namespace opp_lora_relay_official {

Define_Module(LNS);

void LNS::initialize()
{
    keeper = check_and_cast<Keeper *>(getParentModule()->getSubmodule("keeper"));
    dedupHoldoff = par("dedupHoldoff");
    relayForwardDelay = par("ts011RelayForwardDelay").doubleValue();
    networkTimeCorrectionError.setName("TS011 network time correction error");
}

void LNS::finish()
{
    for (auto &item : dedup) {
        if (item.second.timer != nullptr)
            cancelAndDelete(item.second.timer);
        delete item.second.representative;
    }
    dedup.clear();

    recordScalar("TS011 network time correction samples", networkTimeCorrectionError.getCount());
    recordScalar("TS011 mean network time correction error",
                 networkTimeCorrectionError.getCount() ? networkTimeCorrectionError.getMean() : 0);
    if (receivedFrames.empty())
        return;
    std::ifstream existing("exec_result_frames.txt");
    const bool needsHeader = !existing.good();
    existing.close();
    std::ofstream output("exec_result_frames.txt", std::ios::app);
    if (!output)
        return;
    if (needsHeader)
        output << "x,y,sf,rssi,rssi_margin\n";
    output << std::fixed << std::setprecision(2);
    for (const auto &frame : receivedFrames)
        output << frame.x << ',' << frame.y << ',' << frame.sf << ','
               << frame.rssi << ',' << frame.rssi_margin << '\n';
}

void LNS::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage() && msg->getKind() == KIND_DEDUP_TIMER) {
        onDedupTimer(msg);
        return;
    }
    auto *packet = dynamic_cast<LoRaPacket *>(msg);
    if (packet == nullptr) {
        delete msg;
        return;
    }
    if (msg->isSelfMessage()) {
        send(packet, "json$o", packet->gateLNS);
        keeper->recordMessageTransmission(0, ++packetCounter, false);
        return;
    }
    if (msg->arrivedOn("json$i")) {
        onUplinkArrived(packet, msg->getArrivalGate()->getIndex());
        return;
    }
    delete msg;
}

void LNS::calcADR(LoRaPacket *packet, double rssiMargin)
{
    if (rssiMargin > par("ADRtarget").doubleValue() + par("ADRtolerance").doubleValue())
        packet->addPar("ADR-");
    if (rssiMargin < par("ADRtarget").doubleValue() - par("ADRtolerance").doubleValue())
        packet->addPar("ADR+");
}

void LNS::onUplinkArrived(LoRaPacket *packet, int inGateIdx)
{
    if (!packet->hasPar("KeeperIndex") || !packet->hasPar("PacketId")) {
        delete packet;
        return;
    }
    const long deviceId = packet->par("KeeperIndex").longValue();
    const long packetId = packet->par("PacketId").longValue();
    // Distinct legal retries may require fresh ACKs, without duplicate delivery.
    const std::string key = std::to_string(deviceId) + ":" + std::to_string(packetId) + ":" +
        std::to_string(packet->hasPar("AttemptId") ? packet->par("AttemptId").longValue() : 0) + ":" +
        std::to_string(packet->isRelayed ? packet->forwardingRelayId : -1);
    avgRssiByDevGate[deviceId][inGateIdx].add(packet->rssi);

    auto found = dedup.find(key);
    if (found != dedup.end()) {
        found->second.receptions.push_back({inGateIdx, packet->rssi});
        delete packet;
        return;
    }

    DedupEntry entry;
    entry.devIdx = deviceId;
    entry.pktId = packetId;
    entry.t0 = simTime();
    entry.representative = packet;
    entry.receptions.push_back({inGateIdx, packet->rssi});
    entry.timer = new cMessage("dedupTimer", KIND_DEDUP_TIMER);
    entry.timer->addPar("devIdx") = deviceId;
    entry.timer->addPar("pktId") = packetId;
    entry.timer->addPar("dedupKey") = key.c_str();
    scheduleAt(simTime() + dedupHoldoff, entry.timer);
    dedup.emplace(key, std::move(entry));
}

void LNS::onDedupTimer(cMessage *timer)
{
    const long deviceId = timer->par("devIdx").longValue();
    const long packetId = timer->par("pktId").longValue();
    const std::string key = timer->par("dedupKey").stringValue();
    auto found = dedup.find(key);
    if (found == dedup.end()) {
        delete timer;
        return;
    }
    DedupEntry entry = std::move(found->second);
    dedup.erase(found);
    delete timer;

    int bestGate = 0;
    double bestMean = -std::numeric_limits<double>::infinity();
    for (const auto &reception : entry.receptions) {
        const double mean = avgRssiByDevGate[deviceId][reception.gateIdx].mean();
        if (mean > bestMean) {
            bestMean = mean;
            bestGate = reception.gateIdx;
        }
    }
    processUniqueUplink(entry, bestGate);
}

void LNS::processUniqueUplink(DedupEntry &entry, int outputGate)
{
    LoRaPacket *uplink = entry.representative;
    keeper->recordMessageReception(uplink->par("KeeperIndex"), uplink->par("PacketId"));
    const bool forwarded = uplink->encapsulated && uplink->fPort == LA_FPORT_RELAY;
    if (forwarded && uplink->hasPar("OriginalUplinkEnd")) {
        const double estimatedOriginalEnd = entry.t0.dbl() - uplink->calcMsgDuration() - relayForwardDelay;
        networkTimeCorrectionError.collect(estimatedOriginalEnd -
            uplink->par("OriginalUplinkEnd").doubleValue());
    }
    const double accessRssi = forwarded ? uplink->uplinkRssiMetadata : uplink->rssi;
    const double accessMargin = forwarded ? accessRssi + 114.0 : uplink->rssi_margin;
    if (deliveredApplicationFrames.insert(makeKey(entry.devIdx, entry.pktId)).second)
        receivedFrames.push_back({forwarded ? uplink->uplinkSenderPositionX : uplink->senderPositionX,
        forwarded ? uplink->uplinkSenderPositionY : uplink->senderPositionY,
        static_cast<short>(forwarded ? uplink->originalSpreadingFactor : uplink->spreadingFactor),
        accessRssi, accessMargin});

    const int logicalKind = forwarded ? uplink->originalKind : uplink->getKind();
    auto *response = uplink->dupLoRa();
    response->gateLNS = outputGate;
    response->isUplink = false;
    response->isRelayed = false;
    response->relayRequired = uplink->isRelayed;
    bool sendResponse = false;

    if (logicalKind == MsgType::JOINREQUEST) {
        response->setKind(MsgType::JOINACCEPT);
        response->setName("JOIN ACCEPT");
        response->length = 17;
        sendResponse = true;
    }
    else if (logicalKind == MsgType::UPLINK && uplink->ackRequired) {
        response->setKind(MsgType::ACK);
        response->setName("ACK");
        response->length = 12;
        calcADR(response, accessMargin);
        sendResponse = true;
    }

    if (sendResponse && forwarded) {
        const int innerPhyLength = response->phyPayloadLength();
        response->originalKind = response->getKind();
        response->originalLength = response->length;
        response->originalSpreadingFactor = uplink->originalSpreadingFactor;
        response->setKind(MsgType::FORWARDDOWNLINK);
        response->setName("TS011 ForwardDownlinkReq");
        response->encapsulated = true;
        response->fPort = LA_FPORT_RELAY;
        response->length = innerPhyLength;
        response->relayRequired = true;
    }

    if (sendResponse) {
        const double txEnd = uplink->hasPar("RadioTxEnd") ? uplink->par("RadioTxEnd").doubleValue() : entry.t0.dbl();
        const double delay = par(!forwarded && logicalKind == MsgType::JOINREQUEST ?
            "joinAcceptDelay1" : "receiveDelay1").doubleValue();
        const auto target = SimTime(txEnd + delay);
        if (simTime() > target) { delete response; delete uplink; return; }
        if (response->hasPar("TxDeadline")) response->par("TxDeadline") = (target + SimTime(1, SIMTIME_MS)).dbl();
        else response->addPar("TxDeadline") = (target + SimTime(1, SIMTIME_MS)).dbl();
        scheduleAt(target, response);
    }
    else
        delete response;
    delete uplink;
}

} // namespace opp_lora_relay_official
