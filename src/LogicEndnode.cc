#include "LogicEndnode.h"

#include <algorithm>
#include <cmath>

#include "LoRaPacket.h"

namespace opp_lora_relay_official {

Define_Module(LogicEndnode);

namespace {
constexpr long long DAY_MS = 24LL * 60LL * 60LL * 1000LL;
}

LogicEndnode::~LogicEndnode()
{
    for (auto *timer : windowTimers)
        if (timer != nullptr)
            cancelAndDelete(timer);
    if (responseTimer != nullptr)
        cancelAndDelete(responseTimer);
}

void LogicEndnode::initialize()
{
    transmittingWindowStart[0] = par("transmittingWindowStart1").intValue();
    transmittingWindowStart[1] = par("transmittingWindowStart2").intValue();
    transmittingWindowDuration[0] = par("transmittingWindowDuration1").intValue();
    transmittingWindowDuration[1] = par("transmittingWindowDuration2").intValue();
    windowsPerDay = std::max(1, std::min(2, static_cast<int>(par("windowsPerDay").intValue())));
    framesPerWindowMin = std::max(1, static_cast<int>(par("framesPerWindowMin").intValue()));
    framesPerWindowMax = std::max(framesPerWindowMin, static_cast<int>(par("framesPerWindowMax").intValue()));
    applicationPayloadLength = par("applicationPayloadLength").intValue();
    missedPacketsToSearchRelay = std::max(1, static_cast<int>(par("missedPacketsToSearchRelay").intValue()));
    forceSpreadingFactor = par("forceSF").intValue();
    model = static_cast<RetransmissionModel>(par("retransmissionModel").intValue());

    ts011EndDeviceSpreadingFactor = par("ts011EndDeviceSpreadingFactor").intValue();
    ts011DefaultWorChannel = par("ts011DefaultWorChannel").intValue();
    ts011SecondWorChannel = par("ts011SecondWorChannel").intValue();
    ts011UplinkChannel = par("ts011UplinkChannel").intValue();
    ts011SecondChannelConfigured = par("ts011SecondChannelEnabled").boolValue();
    ts011CadPeriodicity = par("ts011CadPeriodicity").doubleValue();
    ts011CadToRxSymbols = par("ts011CadToRxSymbols").intValue();
    ts011WorAckDelay = par("ts011WorAckDelay").doubleValue();
    ts011WorDataDelay = par("ts011WorDataDelay").doubleValue();
    ts011WorValidityProbability = par("ts011WorValidityProbability").doubleValue();
    ts011RelayClockAccuracyPpm = par("ts011RelayClockAccuracyPpm").intValue();
    ts011EndDeviceClockAccuracyPpm = par("ts011EndDeviceClockAccuracyPpm").intValue();
    knownCadPeriodicity = 1.0; // mandatory initialized-state assumption
    if (ts011WorValidityProbability < 0 || ts011WorValidityProbability > 1)
        throw cRuntimeError("ts011WorValidityProbability must be in [0,1]");
    if (applicationPayloadLength < 1 || applicationPayloadLength > 255)
        throw cRuntimeError("applicationPayloadLength must be in [1,255]");

    keeper = check_and_cast<Keeper *>(getParentModule()->getParentModule()->getSubmodule("keeper"));
    myKeeperIndex = keeper->registerEndnode();
    wakeupRelay = registerSignal("wakeupRelay");
    lastAckTime = simTime();
    lastOnline = simTime();
    updateLogicalState(EndDeviceState::OFFLINE);

    for (int i = 0; i < windowsPerDay; ++i) {
        framesRemaining[i] = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;
        windowTimers[i] = new cMessage(i == 0 ? "applicationWindow1" : "applicationWindow2",
                                       i == 0 ? SelfMsgType::TXWINDOW1 : SelfMsgType::TXWINDOW2);
        scheduleInitialWindow(windowTimers[i], i);
    }

    WATCH(endDeviceState);
    WATCH(syncState);
    WATCH(consecutiveWorWithoutAck);
    WATCH(worTransmitted);
    WATCH(worAckReceived);
}

void LogicEndnode::scheduleInitialWindow(cMessage *timer, int windowIndex)
{
    const long long nowMs = simTime().inUnit(SIMTIME_MS);
    const long long dayStartMs = (nowMs / DAY_MS) * DAY_MS;
    long long startMs = dayStartMs + transmittingWindowStart[windowIndex] * 60000LL;
    const long long durationMs = std::max(1LL, transmittingWindowDuration[windowIndex] * 60000LL);
    if (startMs + durationMs <= nowMs)
        startMs += DAY_MS;
    const long long earliest = std::max(nowMs + 1, startMs);
    const long long latest = startMs + durationMs;
    const long long offset = latest > earliest ? intrand(static_cast<int>(latest - earliest)) : 0;
    scheduleAt(SimTime(earliest + offset, SIMTIME_MS), timer);
}

void LogicEndnode::scheduleFollowingWindow(cMessage *timer, int windowIndex)
{
    const long long nowMs = simTime().inUnit(SIMTIME_MS);
    const long long dayStartMs = (nowMs / DAY_MS) * DAY_MS;
    const long long startMs = dayStartMs + transmittingWindowStart[windowIndex] * 60000LL;
    const long long endMs = startMs + std::max(1LL, transmittingWindowDuration[windowIndex] * 60000LL);
    --framesRemaining[windowIndex];
    if (framesRemaining[windowIndex] > 0 && nowMs + 1 < endMs) {
        const long long remaining = endMs - nowMs - 1;
        scheduleAt(SimTime(nowMs + 1 + intrand(static_cast<int>(remaining)), SIMTIME_MS), timer);
        return;
    }
    framesRemaining[windowIndex] = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;
    const long long nextStart = startMs + DAY_MS;
    const long long duration = std::max(1LL, transmittingWindowDuration[windowIndex] * 60000LL);
    scheduleAt(SimTime(nextStart + intrand(static_cast<int>(duration)), SIMTIME_MS), timer);
}

void LogicEndnode::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        switch (msg->getKind()) {
            case SelfMsgType::TXWINDOW1:
            case SelfMsgType::TXWINDOW2: {
                const int index = msg->getKind() == SelfMsgType::TXWINDOW1 ? 0 : 1;
                scheduleFollowingWindow(msg, index);
                beginApplicationTransmission();
                return;
            }
            case SelfMsgType::TS011_SEND_WOR:
                sendTs011Wor(msg);
                return;
            case SelfMsgType::TS011_SEND_DATA: {
                const int packetId = msg->par("packetId").longValue();
                const bool join = msg->par("join").boolValue();
                const bool ack = msg->par("ackRequired").boolValue();
                const bool relayed = msg->par("relayed").boolValue();
                if (model == RetransmissionModel::TS011 && relayed)
                    updateTs011MissedAck(pendingWorCounter);
                sendDataFrame(packetId, join, ack, relayed);
                delete msg;
                return;
            }
            case SelfMsgType::RESPONSE_TIMEOUT:
                handleResponseTimeout(msg);
                return;
            default:
                delete msg;
                return;
        }
    }

    auto *packet = dynamic_cast<LoRaPacket *>(msg);
    if (packet == nullptr) {
        delete msg;
        return;
    }
    if (packet->isWorAck())
        handleWorAck(packet);
    else
        handleDownlink(packet);
}

void LogicEndnode::beginApplicationTransmission()
{
    const int packetId = packetCounter++;
    const bool ackRequired = packetId % 5 == 0 || lastAckRequestCount > 0;
    const bool join = endDeviceState == EndDeviceState::OFFLINE ||
                      endDeviceState == EndDeviceState::JOIN_DIRECT ||
                      endDeviceState == EndDeviceState::JOIN_RELAY;
    const bool relayed = endDeviceState == EndDeviceState::JOIN_RELAY ||
                         endDeviceState == EndDeviceState::ONLINE_RELAYED;

    if (!relayed) {
        if (join)
            updateLogicalState(EndDeviceState::JOIN_DIRECT);
        sendDataFrame(packetId, join, true, false);
        return;
    }

    if (model == RetransmissionModel::TS011) {
        scheduleTs011Wor(packetId, join, ackRequired);
        return;
    }

    const double driftWindow = std::max(0.0, (simTime() - lastAckTime).dbl() *
        par("clockDriftMax").doubleValue() / 1000000.0);
    emit(wakeupRelay, driftWindow * 2, this);
    auto *event = new cMessage("EP431 data", SelfMsgType::TS011_SEND_DATA);
    event->addPar("packetId") = packetId;
    event->addPar("join") = join;
    event->addPar("ackRequired") = ackRequired;
    event->addPar("relayed") = true;
    scheduleAt(simTime() + driftWindow, event);
}

int LogicEndnode::chooseWorChannel() const
{
    if (syncState == Ts011SyncState::SYNCHRONIZED && ts011SecondChannelConfigured)
        return ts011SecondWorChannel;
    return ts011DefaultWorChannel;
}

int LogicEndnode::unsynchronizedPreambleSymbols(int spreadingFactor) const
{
    const double symbol = std::pow(2.0, spreadingFactor) / 125000.0;
    return static_cast<int>(std::floor(knownCadPeriodicity / symbol)) + 1 + 6 + ts011CadToRxSymbols;
}

simtime_t LogicEndnode::synchronizedWorStart(int spreadingFactor, int channel, int &preambleSymbols)
{
    const double symbol = std::pow(2.0, spreadingFactor) / 125000.0;
    const double period = knownCadPeriodicity;
    const double now = simTime().dbl();
    double reference = tRef.dbl();
    if (channel != lastWorChannel)
        reference += period / 2.0;
    double n = std::ceil((now - reference) / period);
    double next = std::max(reference, reference + n * period);
    double drift = (ts011RelayClockAccuracyPpm + ts011EndDeviceClockAccuracyPpm) *
                   std::max(0.0, next - tRef.dbl()) / 1000000.0;
    if (drift > period) {
        syncState = Ts011SyncState::UNSYNCHRONIZED;
        ++syncLosses;
        preambleSymbols = unsynchronizedPreambleSymbols(spreadingFactor);
        return simTime();
    }
    double start = next - drift / 2.0;
    if (start <= now + 0.001) {
        next += period;
        drift = (ts011RelayClockAccuracyPpm + ts011EndDeviceClockAccuracyPpm) *
                std::max(0.0, next - tRef.dbl()) / 1000000.0;
        start = next - drift / 2.0;
    }
    preambleSymbols = std::max(8, static_cast<int>(std::floor(drift / symbol)) +
                                  1 + 6 + ts011CadToRxSymbols);
    return SimTime(start);
}

void LogicEndnode::scheduleTs011Wor(int packetId, bool join, bool ackRequired)
{
    const int channel = chooseWorChannel();
    int preamble = unsynchronizedPreambleSymbols(ts011EndDeviceSpreadingFactor);
    simtime_t start = simTime();
    if (syncState == Ts011SyncState::SYNCHRONIZED)
        start = synchronizedWorStart(ts011EndDeviceSpreadingFactor, channel, preamble);

    auto *event = new cMessage("TS011 WOR transmission", SelfMsgType::TS011_SEND_WOR);
    event->addPar("packetId") = packetId;
    event->addPar("join") = join;
    event->addPar("ackRequired") = ackRequired;
    event->addPar("preambleSymbols") = preamble;
    event->addPar("worChannel") = channel;
    scheduleAt(start, event);
}

void LogicEndnode::sendTs011Wor(cMessage *event)
{
    const int packetId = event->par("packetId").longValue();
    const bool join = event->par("join").boolValue();
    const bool ackRequired = event->par("ackRequired").boolValue();
    const int channel = event->par("worChannel").longValue();
    auto *wor = new LoRaPacket("TS011 WOR", MsgType::MSGWOR);
    wor->isUplink = true;
    wor->relayRequired = true;
    wor->ackRequired = !join;
    wor->worType = join ? WorType::RELAY_JOIN_REQUEST : WorType::CLASS_A_UPLINK;
    wor->length = join ? TS011_JOIN_WOR_LENGTH : TS011_UPLINK_WOR_LENGTH;
    wor->spreadingFactor = ts011EndDeviceSpreadingFactor;
    wor->preambleSymbols = event->par("preambleSymbols").longValue();
    wor->channel = channel;
    wor->worChannel = channel;
    wor->uplinkChannel = ts011UplinkChannel;
    wor->worFrameCounter = worFrameCounter++;
    wor->worValid = uniform(0, 1) <= ts011WorValidityProbability;
    if (!wor->worValid)
        ++invalidWorTransmitted;
    wor->addPar("KeeperIndex") = myKeeperIndex;
    wor->addPar("PacketId") = packetId;
    pendingWorCounter = wor->worFrameCounter;
    pendingWorAcknowledged = false;
    lastWorPreambleSymbols = wor->preambleSymbols;
    lastWorChannel = channel;
    tLast = simTime();
    const double worDuration = wor->calcMsgDuration();
    totalWorPreambleSymbols += wor->preambleSymbols;
    totalWorAirtime += worDuration;
    send(wor, "logicOut");
    ++worTransmitted;

    LoRaPacket ackTiming("WOR ACK timing", MsgType::MSGWORACK);
    ackTiming.length = TS011_WOR_ACK_LENGTH;
    ackTiming.spreadingFactor = ts011EndDeviceSpreadingFactor;
    const double delay = worDuration + (join ? ts011WorDataDelay :
        ts011WorAckDelay + ackTiming.calcMsgDuration() + ts011WorDataDelay);
    auto *data = new cMessage("TS011 subsequent uplink", SelfMsgType::TS011_SEND_DATA);
    data->addPar("packetId") = packetId;
    data->addPar("join") = join;
    data->addPar("ackRequired") = ackRequired;
    data->addPar("relayed") = true;
    scheduleAt(simTime() + delay, data);
    delete event;
}

void LogicEndnode::updateTs011MissedAck(unsigned int)
{
    if (pendingWorAcknowledged) {
        consecutiveWorWithoutAck = 0;
        return;
    }
    ++consecutiveWorWithoutAck;
    if (consecutiveWorWithoutAck < 8)
        return;
    consecutiveWorWithoutAck = 0;
    if (syncState == Ts011SyncState::SYNCHRONIZED) {
        syncState = Ts011SyncState::UNSYNCHRONIZED;
        ++syncLosses;
    }
    else if (syncState == Ts011SyncState::UNSYNCHRONIZED) {
        syncState = Ts011SyncState::INITIALIZED;
        knownCadPeriodicity = 1.0;
        ts011CadToRxSymbols = 8;
        ts011RelayClockAccuracyPpm = 40;
    }
}

void LogicEndnode::sendDataFrame(int packetId, bool join, bool ackRequired, bool relayed)
{
    auto *packet = new LoRaPacket(join ? (relayed ? "JOIN RELAY" : "JOIN DIRECT") : "UPLINK",
                                  join ? MsgType::JOINREQUEST : MsgType::UPLINK);
    packet->id = packetId;
    packet->isUplink = true;
    packet->relayRequired = relayed;
    packet->ackRequired = join || ackRequired;
    packet->rxrEligible = relayed;
    packet->length = join ? (relayed ? RELAY_JOIN_LENGTH : 15) : applicationPayloadLength;
    packet->spreadingFactor = relayed ? ts011EndDeviceSpreadingFactor : forceSpreadingFactor;
    packet->channel = relayed ? ts011UplinkChannel : 1;
    packet->addPar("KeeperIndex") = myKeeperIndex;
    packet->addPar("PacketId") = packetId;
    const bool expectResponse = packet->ackRequired;
    send(packet, "logicOut");
    keeper->recordMessageTransmission(myKeeperIndex, packetId, true);
    waitingResponse = expectResponse;
    if (waitingResponse) {
        ++lastAckRequestCount;
        startResponseTimeout();
    }
}

void LogicEndnode::startResponseTimeout()
{
    if (responseTimer != nullptr) {
        if (responseTimer->isScheduled())
            cancelEvent(responseTimer);
        delete responseTimer;
    }
    responseTimer = new cMessage("response timeout", SelfMsgType::RESPONSE_TIMEOUT);
    scheduleAt(simTime() + REPLAY_TIMEOUT, responseTimer);
}

void LogicEndnode::handleResponseTimeout(cMessage *msg)
{
    responseTimer = nullptr;
    if (waitingResponse) {
        waitingResponse = false;
        ++missedResponse;
        if (missedResponse >= missedPacketsToSearchRelay) {
            missedResponse = 0;
            lastAckRequestCount = 0;
            updateLogicalState(keeper->hasRelayPath() ? EndDeviceState::JOIN_RELAY : EndDeviceState::OFFLINE);
        }
        send(new cMessage("ResetADR", MsgType::RESET_ADR), "logicOut");
    }
    delete msg;
}

void LogicEndnode::handleWorAck(LoRaPacket *packet)
{
    if (packet->worFrameCounter == pendingWorCounter) {
        pendingWorAcknowledged = true;
        lastAcknowledgedWorCounter = packet->worFrameCounter;
        consecutiveWorWithoutAck = 0;
        knownCadPeriodicity = packet->cadPeriodicityMs / 1000.0;
        ts011CadToRxSymbols = packet->cadToRxSymbols;
        ts011RelayClockAccuracyPpm = packet->relayClockAccuracyPpm;
        tRef = tLast + lastWorPreambleSymbols * packet->symbolTime() - packet->tOffsetMs / 1000.0;
        syncState = Ts011SyncState::SYNCHRONIZED;
        ++worAckReceived;
    }
    delete packet;
}

void LogicEndnode::handleDownlink(LoRaPacket *packet)
{
    if (packet->hasPar("KeeperIndex") && packet->hasPar("PacketId"))
        keeper->recordDownlinkReception(packet->par("KeeperIndex").longValue(),
                                        packet->par("PacketId").longValue());
    missedResponse = 0;
    waitingResponse = false;
    lastAckRequestCount = 0;
    lastAckTime = simTime();
    if (responseTimer != nullptr) {
        if (responseTimer->isScheduled())
            cancelEvent(responseTimer);
        delete responseTimer;
        responseTimer = nullptr;
    }
    if (packet->getKind() == MsgType::JOINACCEPT) {
        if (endDeviceState == EndDeviceState::JOIN_RELAY) {
            updateLogicalState(EndDeviceState::ONLINE_RELAYED);
            if (syncState == Ts011SyncState::INITIALIZED)
                syncState = Ts011SyncState::UNSYNCHRONIZED;
        }
        else {
            updateLogicalState(EndDeviceState::ONLINE_DIRECT);
        }
    }
    else if (packet->getKind() == MsgType::TIMEOUT) {
        updateLogicalState(EndDeviceState::OFFLINE);
    }
    delete packet;
}

void LogicEndnode::recordOnline()
{
    const double offlineTime = (simTime() - lastOnline).dbl();
    offlineMax = std::max(offlineMax, offlineTime);
    offlineTotal += offlineTime;
    keeper->recordOfflineTime(myKeeperIndex, offlineTime);
}

void LogicEndnode::updateLogicalState(EndDeviceState state)
{
    if (endDeviceState != state)
        keeper->updateEndnodeState(myKeeperIndex, state);
    endDeviceState = state;
    cDisplayString &display = getParentModule()->getDisplayString();
    switch (state) {
        case EndDeviceState::OFFLINE:
            display.setTagArg("i2", 0, "status/excl2");
            lastOnline = simTime();
            break;
        case EndDeviceState::JOIN_DIRECT:
            display.setTagArg("i2", 0, "status/question");
            break;
        case EndDeviceState::JOIN_RELAY:
            display.setTagArg("i2", 0, "status/bulb");
            break;
        case EndDeviceState::ONLINE_DIRECT:
            display.setTagArg("i2", 0, "status/green");
            recordOnline();
            break;
        case EndDeviceState::ONLINE_RELAYED:
            display.setTagArg("i2", 0, "status/yellow");
            recordOnline();
            break;
    }
}

void LogicEndnode::finish()
{
    recordOnline();
    recordScalar("TS011 WOR transmitted", worTransmitted);
    recordScalar("TS011 WOR ACK received", worAckReceived);
    recordScalar("TS011 synchronization losses", syncLosses);
    recordScalar("TS011 invalid WOR transmitted", invalidWorTransmitted);
    recordScalar("TS011 final synchronization state", static_cast<int>(syncState));
    recordScalar("TS011 mean WOR preamble symbols",
                 worTransmitted > 0 ? totalWorPreambleSymbols / worTransmitted : 0);
    recordScalar("TS011 total WOR airtime", totalWorAirtime);
}

} // namespace opp_lora_relay_official
