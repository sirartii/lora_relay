#include "LogicEndnode.h"

#include <algorithm>
#include <cmath>

#include "LoRaPacket.h"
#include "WindowSchedule.h"

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
    if (nextTransactionTimer != nullptr)
        cancelAndDelete(nextTransactionTimer);
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
    ackEvery = std::max(1, static_cast<int>(par("ackEvery").intValue()));
    ts011MissedAcksToRelay = std::max(1, static_cast<int>(par("ts011MissedAcksToRelay").intValue()));
    ts011JoinRelayEvery = std::max(1, static_cast<int>(par("ts011JoinRelayEvery").intValue()));
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
    if (startMs + durationMs <= nowMs + 1)
        startMs += DAY_MS;
    windowStartMs[windowIndex] = startMs;
    const long long earliest = std::max(nowMs + 1, startMs);
    planWindow(timer, windowIndex, earliest, startMs + durationMs);
}

void LogicEndnode::planWindow(cMessage *timer, int windowIndex, long long firstMs, long long endMs)
{
    const int count = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;
    windowDeadlinesMs[windowIndex] = sampleWindowSchedule(firstMs, endMs, count,
        [this](int width) { return intrand(width); });
    nextWindowDeadline[windowIndex] = 0;
    scheduleAt(SimTime(windowDeadlinesMs[windowIndex].front(), SIMTIME_MS), timer);
}

void LogicEndnode::scheduleFollowingWindow(cMessage *timer, int windowIndex)
{
    auto& next = nextWindowDeadline[windowIndex];
    const auto& deadlines = windowDeadlinesMs[windowIndex];
    if (++next < deadlines.size()) {
        scheduleAt(SimTime(deadlines[next], SIMTIME_MS), timer);
        return;
    }
    windowStartMs[windowIndex] += DAY_MS;
    const long long nextStart = windowStartMs[windowIndex];
    const long long duration = std::max(1LL, transmittingWindowDuration[windowIndex] * 60000LL);
    planWindow(timer, windowIndex, nextStart, nextStart + duration);
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
                bool relayed = msg->par("relayed").boolValue();
                if (!transactionActive || packetId != activePacketId) {
                    delete msg;
                    return;
                }
                if (model == RetransmissionModel::TS011 && relayed && !join)
                    updateTs011MissedAck(pendingWorCounter);
                if (model == RetransmissionModel::TS011 && !join &&
                    (relayForwardingDisabled || simTime() < relayHoldoffUntil))
                    relayed = false; // Forward says RL cannot help; keep the direct opportunity.
                activeRelayed = relayed;
                sendDataFrame(packetId, join, ack, relayed);
                delete msg;
                return;
            }
            case SelfMsgType::RESPONSE_TIMEOUT:
                handleResponseTimeout(msg);
                return;
            case SelfMsgType::MAC_START_NEXT:
                nextTransactionTimer = nullptr;
                delete msg;
                if (transactionActive)
                    startActiveAttempt();
                else
                    startNextTransmission();
                return;
            default:
                delete msg;
                return;
        }
    }

    if (msg->getKind() == MsgType::MAC_TX_COMPLETE) {
        startResponseTimeout(msg);
        delete msg;
        return;
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
    pendingApplicationTimes.push_back(simTime());
    if (!transactionActive && nextTransactionTimer == nullptr)
        startNextTransmission();
}

void LogicEndnode::startNextTransmission()
{
    if (transactionActive || pendingApplicationTimes.empty())
        return;
    activeApplicationTime = pendingApplicationTimes.front();
    pendingApplicationTimes.pop_front();
    totalMacQueueDelay += (simTime() - activeApplicationTime).dbl();
    transactionActive = true;
    activePacketId = packetCounter++;
    activeAttempt = 0;
    activeFallbackUsed = false;
    activeJoin = !sessionJoined;
    activeAckRequired = activeJoin || (++dataCounter % ackEvery == 0);
    if (activeJoin) {
        ++joinAttemptCounter;
        if (model == RetransmissionModel::TS011)
            relayMode = keeper->hasRelayPath() && joinAttemptCounter % ts011JoinRelayEvery == 0;
    }
    activeRelayed = relayMode && keeper->hasRelayPath();
    if (!activeJoin && model == RetransmissionModel::TS011 &&
        (relayForwardingDisabled || simTime() < relayHoldoffUntil))
        activeRelayed = false;
    updateSessionState();
    startActiveAttempt();
}

void LogicEndnode::startActiveAttempt()
{
    const int packetId = activePacketId;
    const bool join = activeJoin;
    const bool ackRequired = activeAckRequired;
    const bool relayed = activeRelayed;

    if (!relayed) {
        sendDataFrame(packetId, join, ackRequired, false);
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

void LogicEndnode::updateSessionState()
{
    const auto desired = sessionJoined ? (relayMode ? EndDeviceState::ONLINE_RELAYED : EndDeviceState::ONLINE_DIRECT)
                                      : (relayMode ? EndDeviceState::JOIN_RELAY : EndDeviceState::JOIN_DIRECT);
    if (endDeviceState != desired) updateLogicalState(desired);
}

void LogicEndnode::completeTransaction()
{
    transactionActive = false;
    waitingResponse = false;
    activePacketId = -1;
    if (responseTimer != nullptr) {
        cancelAndDelete(responseTimer);
        responseTimer = nullptr;
    }
    if (!pendingApplicationTimes.empty() && nextTransactionTimer == nullptr) {
        nextTransactionTimer = new cMessage("next MAC transaction", SelfMsgType::MAC_START_NEXT);
        scheduleAt(simTime(), nextTransactionTimer);
    }
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
    int channel = chooseWorChannel();
    int preamble = unsynchronizedPreambleSymbols(ts011EndDeviceSpreadingFactor);
    simtime_t start = simTime();
    if (syncState == Ts011SyncState::SYNCHRONIZED)
        start = synchronizedWorStart(ts011EndDeviceSpreadingFactor, channel, preamble);
    // Drift may have invalidated synchronization in synchronizedWorStart().
    // The first unsynchronized WOR already belongs on the default channel.
    if (syncState != Ts011SyncState::SYNCHRONIZED)
        channel = ts011DefaultWorChannel;

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
    wor->joinEui = 0x0102030405060708ULL;
    wor->devEui = static_cast<unsigned int>(myKeeperIndex);
    wor->worValid = uniform(0, 1) <= ts011WorValidityProbability;
    if (!wor->worValid)
        ++invalidWorTransmitted;
    wor->addPar("KeeperIndex") = myKeeperIndex;
    wor->addPar("PacketId") = packetId;
    wor->addPar("AttemptId") = activeAttempt;
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
    packet->length = join ? 23 : applicationPayloadLength;
    packet->spreadingFactor = relayed ? ts011EndDeviceSpreadingFactor : forceSpreadingFactor;
    packet->channel = relayed ? ts011UplinkChannel : 1;
    packet->joinEui = 0x0102030405060708ULL;
    packet->devEui = static_cast<unsigned int>(myKeeperIndex);
    packet->addPar("KeeperIndex") = myKeeperIndex;
    packet->addPar("PacketId") = packetId;
    packet->addPar("AttemptId") = activeAttempt;
    packet->addPar("ApplicationTime") = activeApplicationTime.dbl();
    const bool expectResponse = packet->ackRequired;
    send(packet, "logicOut");
    keeper->recordMessageTransmission(myKeeperIndex, packetId, true);
    keeper->recordFrameDetails(myKeeperIndex, packetId, join, ackRequired, relayed, activeApplicationTime);
    waitingResponse = expectResponse;
    if (join) ++joinTransmitted;
    else if (ackRequired) ++confirmedDataTransmitted;
}

void LogicEndnode::startResponseTimeout(cMessage *txComplete)
{
    if (!transactionActive || txComplete->par("PacketId").longValue() != activePacketId ||
        txComplete->par("AttemptId").longValue() != activeAttempt)
        return;
    if (responseTimer != nullptr) {
        if (responseTimer->isScheduled())
            cancelEvent(responseTimer);
        delete responseTimer;
    }
    responseTimer = new cMessage("response timeout", SelfMsgType::RESPONSE_TIMEOUT);
    responseTimer->addPar("PacketId") = activePacketId;
    responseTimer->addPar("AttemptId") = activeAttempt;
    const double lastDelay = activeRelayed ?
        par(model == RetransmissionModel::TS011 ? "ts011RxrDelay" : "epRxrDelay").doubleValue() :
        par(activeJoin ? "joinAcceptDelay1" : "receiveDelay1").doubleValue() + 1.0;
    LoRaPacket reply("reply timing", activeJoin ? MsgType::JOINACCEPT : MsgType::ACK);
    reply.length = activeJoin ? 17 : 12;
    reply.spreadingFactor = txComplete->par("sf").longValue();
    // Reception begun at the end of a window is allowed to finish.
    scheduleAt(simTime() + lastDelay + par("receiveWindowDuration").doubleValue() +
               reply.calcMsgDuration() + SimTime(1, SIMTIME_MS), responseTimer);
}

void LogicEndnode::handleResponseTimeout(cMessage *msg)
{
    responseTimer = nullptr;
    if (!transactionActive || msg->par("PacketId").longValue() != activePacketId ||
        msg->par("AttemptId").longValue() != activeAttempt) {
        delete msg;
        return;
    }
    if (waitingResponse) {
        waitingResponse = false;
        ++missedResponse;
        const int threshold = model == RetransmissionModel::TS011 ? ts011MissedAcksToRelay : missedPacketsToSearchRelay;
        const bool restrictedOut = model == RetransmissionModel::EP431 &&
                                   !keeper->epRelayEligible(myKeeperIndex);
        if (restrictedOut && activeJoin && activeRelayed) {
            // One unanswered Relay Join is enough to leave relay acquisition.
            // Next scheduled opportunity sends an ordinary direct Join.
            sessionJoined = false;
            relayMode = false;
            missedResponse = 0;
            keeper->recordEpRecovery(myKeeperIndex, activePacketId, 2);
            updateSessionState();
        }
        else if (!activeJoin && missedResponse >= threshold && keeper->hasRelayPath()) {
            if (restrictedOut) {
                // The ED does not obtain a designation. It first requests one
                // using Relay Join, rather than sending unaccepted relay data.
                sessionJoined = false;
                keeper->recordEpRecovery(myKeeperIndex, activePacketId, 1);
            }
            relayMode = true;
            missedResponse = 0;
            updateSessionState();
            if (model == RetransmissionModel::TS011 && !activeRelayed && !activeFallbackUsed &&
                !relayForwardingDisabled && simTime() >= relayHoldoffUntil) {
                activeFallbackUsed = true;
                activeRelayed = true;
                ++activeAttempt;
                ++relayFallbacks;
                nextTransactionTimer = new cMessage("TS011 confirmed fallback", SelfMsgType::MAC_START_NEXT);
                scheduleAt(simTime() + uniform(1.0, 3.0), nextTransactionTimer);
                delete msg;
                return;
            }
        }
        else if (activeJoin && model != RetransmissionModel::TS011 &&
                 missedResponse >= missedPacketsToSearchRelay) {
            relayMode = keeper->hasRelayPath();
            missedResponse = 0;
            updateSessionState();
        }
        send(new cMessage("ResetADR", MsgType::RESET_ADR), "logicOut");
    }
    delete msg;
    completeTransaction();
}

void LogicEndnode::handleWorAck(LoRaPacket *packet)
{
    if (transactionActive && !activeJoin && packet->worFrameCounter == pendingWorCounter &&
        packet->hasPar("PacketId") && packet->par("PacketId").longValue() == activePacketId &&
        packet->par("AttemptId").longValue() == activeAttempt) {
        pendingWorAcknowledged = true;
        lastAcknowledgedWorCounter = packet->worFrameCounter;
        consecutiveWorWithoutAck = 0;
        knownCadPeriodicity = packet->cadPeriodicityMs / 1000.0;
        ts011CadToRxSymbols = packet->cadToRxSymbols;
        ts011RelayClockAccuracyPpm = packet->relayClockAccuracyPpm;
        tRef = tLast + lastWorPreambleSymbols * packet->symbolTime() - packet->tOffsetMs / 1000.0;
        syncState = Ts011SyncState::SYNCHRONIZED;
        ++worAckReceived;
        if (packet->forwardStatus != 0) {
            ++forwardingHoldoffs;
            if (packet->forwardStatus == 3)
                relayForwardingDisabled = true; // No runtime reconfiguration in this subset.
            else
                relayHoldoffUntil = std::max(relayHoldoffUntil, simTime() +
                    (packet->forwardStatus == 1 ? 1800 : 3600));
        }
    }
    else ++ignoredResponses;
    delete packet;
}

void LogicEndnode::handleDownlink(LoRaPacket *packet)
{
    const bool matches = transactionActive && packet->hasPar("PacketId") && packet->hasPar("AttemptId") &&
        packet->par("PacketId").longValue() == activePacketId &&
        packet->par("AttemptId").longValue() == activeAttempt;
    const bool expectedKind = activeJoin ? packet->getKind() == MsgType::JOINACCEPT :
        (packet->getKind() == MsgType::ACK || (!activeAckRequired && packet->getKind() == MsgType::DOWNLINK));
    if (!matches || !expectedKind) {
        ++ignoredResponses;
        delete packet;
        return;
    }
    if (packet->hasPar("KeeperIndex") && packet->hasPar("PacketId"))
        keeper->recordDownlinkReception(packet->par("KeeperIndex").longValue(),
                                        packet->par("PacketId").longValue());
    missedResponse = 0;
    waitingResponse = false;
    lastAckTime = simTime();
    if (forceSpreadingFactor == -1) {
        if (packet->hasPar("ADR-"))
            send(new cMessage("ADR decrease SF", MsgType::ADR_DECREASE_SF), "logicOut");
        else if (packet->hasPar("ADR+"))
            send(new cMessage("ADR increase SF", MsgType::ADR_INCREASE_SF), "logicOut");
    }
    if (responseTimer != nullptr) {
        if (responseTimer->isScheduled())
            cancelEvent(responseTimer);
        delete responseTimer;
        responseTimer = nullptr;
    }
    if (packet->getKind() == MsgType::JOINACCEPT) {
        sessionJoined = true;
        relayMode = packet->hasPar("ReceiveWindow") && packet->par("ReceiveWindow").longValue() == 3;
        if (relayMode) {
            if (syncState == Ts011SyncState::INITIALIZED)
                syncState = Ts011SyncState::UNSYNCHRONIZED;
            consecutiveWorWithoutAck = 0;
        }
        updateSessionState();
    }
    delete packet;
    completeTransaction();
}

void LogicEndnode::recordOnline()
{
    if (lastOnline < SIMTIME_ZERO) return;
    const double offlineTime = (simTime() - lastOnline).dbl();
    offlineMax = std::max(offlineMax, offlineTime);
    offlineTotal += offlineTime;
    keeper->recordOfflineTime(myKeeperIndex, offlineTime);
    lastOnline = SimTime(-1);
}

void LogicEndnode::updateLogicalState(EndDeviceState state)
{
    const bool wasOnline = endDeviceState == EndDeviceState::ONLINE_DIRECT || endDeviceState == EndDeviceState::ONLINE_RELAYED;
    const bool willBeOnline = state == EndDeviceState::ONLINE_DIRECT || state == EndDeviceState::ONLINE_RELAYED;
    if (wasOnline && !willBeOnline) lastOnline = simTime();
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
    recordScalar("MAC ignored unmatched responses", ignoredResponses);
    recordScalar("MAC confirmed data attempts", confirmedDataTransmitted);
    recordScalar("MAC Join attempts", joinTransmitted);
    recordScalar("MAC direct-to-relay retries", relayFallbacks);
    recordScalar("MAC forwarding holdoffs", forwardingHoldoffs);
    recordScalar("MAC pending application opportunities", pendingApplicationTimes.size());
    recordScalar("MAC total queue delay seconds", totalMacQueueDelay);
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
