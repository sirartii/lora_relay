#include "LogicRelay.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

#include "LoRaPacket.h"

namespace opp_lora_relay_official {

using namespace omnetpp;

Define_Module(LogicRelay);

void LogicRelay::TokenBucket::configure(double rate, double size, simtime_t now)
{
    reloadRate = rate;
    capacity = std::max(0.0, size);
    tokens = rate < 0 ? 0 : std::min(capacity, std::max(0.0, rate));
    lastReload = now;
}

void LogicRelay::TokenBucket::reload(simtime_t now)
{
    if (reloadRate < 0)
        return;
    const long fullHours = static_cast<long>(std::floor((now - lastReload).dbl() / 3600.0));
    if (fullHours > 0) {
        tokens = std::min(capacity, tokens + fullHours * reloadRate);
        lastReload += fullHours * 3600;
    }
}

bool LogicRelay::TokenBucket::available(simtime_t now)
{
    reload(now);
    return reloadRate < 0 || tokens >= 1;
}

bool LogicRelay::TokenBucket::consume(simtime_t now)
{
    if (!available(now))
        return false;
    if (reloadRate >= 0)
        tokens -= 1;
    return true;
}

double LogicRelay::TokenBucket::secondsUntilToken(simtime_t now)
{
    reload(now);
    if (reloadRate < 0 || tokens >= 1)
        return 0;
    return std::max(0.0, 3600.0 - (now - lastReload).dbl());
}

void LogicRelay::initialize()
{
    model = static_cast<RetransmissionModel>(par("retransmissionModel").intValue());
    worAckDelay = par("ts011WorAckDelay").doubleValue();
    relayForwardDelay = par("ts011RelayForwardDelay").doubleValue();
    rxrDelay = par("ts011RxrDelay").doubleValue();
    worAckProbability = par("ts011WorAckProbability").doubleValue();
    cadPeriodicityMs = static_cast<int>(std::lround(par("ts011CadPeriodicity").doubleValue() * 1000));
    cadToRxSymbols = par("ts011CadToRxSymbols").intValue();
    relayClockAccuracyPpm = par("ts011RelayClockAccuracyPpm").intValue();
    relaySpreadingFactor = par("ts011RelaySpreadingFactor").intValue();
    relayUplinkChannel = par("ts011RelayUplinkChannel").intValue();
    rxrChannel = par("ts011RxrChannel").intValue();
    maximumForwardedPayload = par("ts011MaximumForwardedPayload").intValue();
    forwardingEnabled = par("ts011ForwardingEnabled").boolValue();
    parseJoinFilterRules(par("ts011JoinFilterRules").stdstringValue());

    joinBucket.configure(par("ts011JoinReloadRate").doubleValue(),
                         par("ts011JoinBucketSize").doubleValue(), simTime());
    aggregateTrustedBucket.configure(par("ts011TrustedReloadRate").doubleValue(),
                                     par("ts011TrustedBucketSize").doubleValue(), simTime());
    globalBucket.configure(par("ts011GlobalReloadRate").doubleValue(),
                           par("ts011GlobalBucketSize").doubleValue(), simTime());
    perDeviceRate = par("ts011PerDeviceReloadRate").doubleValue();
    perDeviceCapacity = par("ts011PerDeviceBucketSize").doubleValue();

    const int allowedCadMs[] = {1000, 500, 250, 100, 50, 20};
    if (std::find(allowedCadMs, allowedCadMs + 6, cadPeriodicityMs) == allowedCadMs + 6)
        throw cRuntimeError("ts011CadPeriodicity must be one of 1s, 500ms, 250ms, 100ms, 50ms or 20ms");
    if (cadToRxSymbols != 2 && cadToRxSymbols != 4 && cadToRxSymbols != 6 && cadToRxSymbols != 8)
        throw cRuntimeError("ts011CadToRxSymbols must be 2, 4, 6 or 8");
    if (worAckProbability < 0 || worAckProbability > 1)
        throw cRuntimeError("ts011WorAckProbability must be in [0,1]");
    if (maximumForwardedPayload < 1 || maximumForwardedPayload > 255)
        throw cRuntimeError("ts011MaximumForwardedPayload must be in [1,255]");

    WATCH(worReceived);
    WATCH(worAcknowledged);
    WATCH(uplinksForwarded);
    WATCH(downlinksForwarded);
    WATCH(forwardingLimitDrops);
    WATCH(payloadLimitDrops);
    WATCH(joinFilterDrops);
}

void LogicRelay::parseJoinFilterRules(const std::string &rules)
{
    std::stringstream input(rules);
    std::string item;
    while (std::getline(input, item, ',')) {
        if (joinFilterRules.size() >= 16)
            throw cRuntimeError("ts011JoinFilterRules supports at most 16 entries");
        std::stringstream fields(item);
        std::string action;
        std::string lengthText;
        std::string hex;
        std::getline(fields, action, ':');
        std::getline(fields, lengthText, ':');
        std::getline(fields, hex);
        std::transform(action.begin(), action.end(), action.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        JoinFilterRule rule;
        if (action == "forward")
            rule.forward = true;
        else if (action == "filter")
            rule.forward = false;
        else
            throw cRuntimeError("Join filter action must be 'forward' or 'filter'");
        try {
            rule.length = std::stoi(lengthText);
        }
        catch (...) {
            throw cRuntimeError("Invalid Join filter prefix length");
        }
        if (rule.length < 0 || rule.length > 16 || static_cast<int>(hex.size()) != rule.length * 2)
            throw cRuntimeError("Join filter prefix must contain exactly lengthBytes*2 hex digits");
        for (int i = 0; i < rule.length; ++i) {
            try {
                rule.prefix[i] = static_cast<unsigned char>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
            }
            catch (...) {
                throw cRuntimeError("Invalid hexadecimal Join filter prefix");
            }
        }
        joinFilterRules.push_back(rule);
    }
}

bool LogicRelay::joinRequestAllowed(const LoRaPacket *packet) const
{
    std::array<unsigned char, 16> key{};
    for (int i = 0; i < 8; ++i) {
        key[i] = static_cast<unsigned char>((packet->joinEui >> (8 * i)) & 0xff);
        key[8 + i] = static_cast<unsigned char>((packet->devEui >> (8 * i)) & 0xff);
    }
    bool decision = true;
    int longest = -1;
    for (const auto &rule : joinFilterRules) {
        if (rule.length < longest || !std::equal(rule.prefix.begin(), rule.prefix.begin() + rule.length, key.begin()))
            continue;
        longest = rule.length;
        decision = rule.forward;
    }
    return decision;
}

LogicRelay::TokenBucket &LogicRelay::deviceBucket(int deviceId)
{
    auto result = perDeviceBuckets.emplace(deviceId, TokenBucket{});
    if (result.second)
        result.first->second.configure(perDeviceRate, perDeviceCapacity, simTime());
    return result.first->second;
}

bool LogicRelay::canForward(int deviceId, bool joinRequest, bool consume)
{
    if (!forwardingEnabled)
        return false;
    TokenBucket &perDevice = deviceBucket(deviceId);
    const bool available = globalBucket.available(simTime()) &&
        (joinRequest ? joinBucket.available(simTime()) :
                       (perDevice.available(simTime()) && aggregateTrustedBucket.available(simTime())));
    if (!available || !consume)
        return available;
    globalBucket.consume(simTime());
    if (joinRequest)
        joinBucket.consume(simTime());
    else {
        perDevice.consume(simTime());
        aggregateTrustedBucket.consume(simTime());
    }
    return true;
}

int LogicRelay::forwardingStatus(int deviceId)
{
    if (!forwardingEnabled)
        return 3;
    if (canForward(deviceId, false, false))
        return 0;
    const double wait = std::max(globalBucket.secondsUntilToken(simTime()),
        std::max(aggregateTrustedBucket.secondsUntilToken(simTime()),
                 deviceBucket(deviceId).secondsUntilToken(simTime())));
    return wait <= 1800 ? 1 : 2;
}

void LogicRelay::handleWor(LoRaPacket *packet)
{
    ++worReceived;
    if (!packet->worValid) {
        delete packet;
        return;
    }
    WorContext context;
    context.channel = packet->worChannel;
    context.uplinkChannel = packet->uplinkChannel;
    context.counter = packet->worFrameCounter;
    context.receivedAt = simTime();
    worContexts[packet->senderId] = context;

    if (model == RetransmissionModel::TS011 && packet->worType == WorType::CLASS_A_UPLINK &&
        uniform(0, 1) <= worAckProbability) {
        auto *ack = packet->dupLoRa();
        ack->setKind(MsgType::MSGWORACK);
        ack->setName("TS011 WOR ACK");
        ack->isUplink = false;
        ack->isRelayed = true; // marks the relay-to-device access link for the physical model
        ack->relayRequired = false;
        ack->ackRequired = false;
        ack->length = TS011_WOR_ACK_LENGTH;
        ack->channel = packet->worChannel;
        ack->cadPeriodicityMs = cadPeriodicityMs;
        ack->cadToRxSymbols = cadToRxSymbols;
        ack->relayClockAccuracyPpm = relayClockAccuracyPpm;
        ack->forwardStatus = forwardingStatus(packet->senderId);
        ack->relayDataRate = relaySpreadingFactor;
        scheduleAt(simTime() + worAckDelay, ack);
        ++worAcknowledged;
    }
    delete packet;
}

void LogicRelay::handleAccessUplink(LoRaPacket *packet)
{
    if (!packet->relayRequired || packet->isRelayed) {
        delete packet;
        return;
    }
    const bool join = packet->getKind() == MsgType::JOINREQUEST;
    if (model == RetransmissionModel::EP431) {
        packet->isRelayed = true;
        packet->addPar("OriginalUplinkEnd") = simTime().dbl();
        scheduleAt(simTime(), packet);
        ++uplinksForwarded;
        return;
    }

    if (join && !joinRequestAllowed(packet)) {
        ++joinFilterDrops;
        delete packet;
        return;
    }

    if (worContexts.find(packet->senderId) == worContexts.end()) {
        delete packet;
        return;
    }
    if (!canForward(packet->senderId, join, true)) {
        ++forwardingLimitDrops;
        delete packet;
        return;
    }
    const int forwardedLength = packet->length + LORAWAN_FRAME_OVERHEAD + TS011_UPLINK_METADATA_LENGTH;
    if (forwardedLength > maximumForwardedPayload || forwardedLength > 255) {
        ++payloadLimitDrops;
        delete packet;
        return;
    }

    packet->originalKind = packet->getKind();
    packet->originalLength = packet->length;
    packet->originalSpreadingFactor = packet->spreadingFactor;
    packet->uplinkRssiMetadata = packet->rssi;
    packet->uplinkSnrMetadata = packet->snr;
    packet->uplinkSenderPositionX = packet->senderPositionX;
    packet->uplinkSenderPositionY = packet->senderPositionY;
    packet->setKind(MsgType::FORWARDUPLINK);
    packet->setName("TS011 ForwardUplinkReq");
    packet->encapsulated = true;
    packet->fPort = LA_FPORT_RELAY;
    packet->length = forwardedLength;
    packet->spreadingFactor = relaySpreadingFactor;
    packet->channel = relayUplinkChannel;
    packet->isRelayed = true;
    packet->isUplink = true;
    packet->rxrEligible = false;
    packet->addPar("OriginalUplinkEnd") = simTime().dbl();
    scheduleAt(simTime() + relayForwardDelay, packet);
    ++uplinksForwarded;
}

void LogicRelay::handleForwardDownlink(LoRaPacket *packet)
{
    if (model == RetransmissionModel::TS011 &&
        (!packet->encapsulated || packet->fPort != LA_FPORT_RELAY)) {
        delete packet;
        return;
    }
    const double originalEnd = packet->hasPar("OriginalUplinkEnd")
        ? packet->par("OriginalUplinkEnd").doubleValue() : simTime().dbl();
    if (packet->encapsulated) {
        packet->setKind(packet->originalKind);
        packet->length = packet->originalLength;
        packet->spreadingFactor = packet->originalSpreadingFactor;
        packet->encapsulated = false;
        packet->fPort = 0;
    }
    packet->isRelayed = true;
    packet->relayRequired = false;
    packet->isUplink = false;
    packet->channel = rxrChannel;
    const simtime_t target = std::max(simTime(), SimTime(originalEnd + rxrDelay));
    scheduleAt(target, packet);
    ++downlinksForwarded;
}

void LogicRelay::handleMessage(cMessage *msg)
{
    auto *packet = dynamic_cast<LoRaPacket *>(msg);
    if (packet == nullptr) {
        delete msg;
        return;
    }
    if (msg->isSelfMessage()) {
        send(packet, "logicOut");
        return;
    }
    switch (packet->getKind()) {
        case MsgType::MSGWOR:
            handleWor(packet);
            return;
        case MsgType::UPLINK:
        case MsgType::JOINREQUEST:
            handleAccessUplink(packet);
            return;
        case MsgType::FORWARDDOWNLINK:
            handleForwardDownlink(packet);
            return;
        case MsgType::JOINACCEPT:
        case MsgType::ACK:
        case MsgType::DOWNLINK:
            if (model == RetransmissionModel::EP431) {
                handleForwardDownlink(packet);
                return;
            }
            delete packet;
            return;
        default:
            delete packet;
            return;
    }
}

void LogicRelay::finish()
{
    recordScalar("TS011 WOR received", worReceived);
    recordScalar("TS011 WOR acknowledged", worAcknowledged);
    recordScalar("TS011 uplinks forwarded", uplinksForwarded);
    recordScalar("TS011 downlinks forwarded", downlinksForwarded);
    recordScalar("TS011 forwarding-limit drops", forwardingLimitDrops);
    recordScalar("TS011 payload-limit drops", payloadLimitDrops);
    recordScalar("TS011 Join-Request filter drops", joinFilterDrops);
}

} // namespace opp_lora_relay_official
