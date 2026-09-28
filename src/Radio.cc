#include "Radio.h"
#include "WorEnergy.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace opp_lora_relay_official {

Define_Module(GatewayRadio);
Define_Module(EndDeviceRadio);
Define_Module(RelayRadio);
Define_Module(IpBridgeRadio);

Radio::~Radio()
{
    auto dispose = [this](omnetpp::cMessage *message) {
        if (message != nullptr) {
            if (message->isScheduled())
                cancelEvent(message);
            delete message;
        }
    };
    dispose(radioRXTimer);
    dispose(radioUpdateReceive);
    dispose(timerListeningWindowStart1);
    dispose(timerListeningWindowStop1);
    dispose(timerListeningWindowStart2);
    dispose(timerListeningWindowStop2);
    for (auto &pending : pendingWorAcquisitions) {
        dispose(pending.first);
        delete pending.second;
    }
    delete currentRcvMessage;
}

void Radio::initialize()
{
    deviceType = configuredDeviceType();
    cModule *host = getParentModule();
    moduleLabel = host->getFullName();
    const int index = host->isVector() ? host->getIndex() : -1;
    keeper = check_and_cast<Keeper *>(host->getParentModule()->getSubmodule("keeper"));
    myKeeperIndex = keeper->registerMe(host->getName(), index, deviceType, this);

    position.x = host->par("xpos").doubleValue();
    position.y = host->par("ypos").doubleValue();
    sensitivity = par("sensitivityAllRadios").doubleValue();
    txPower = par("txPower").doubleValue();
    retransmissionModel = static_cast<RetransmissionModel>(par("retransmissionModel").intValue());

    listeningWindowStart1 = par("listeningWindowStart1").intValue();
    listeningWindowStart2 = par("listeningWindowStart2").intValue();
    listeningWindowDuration1 = par("listeningWindowDuration1").intValue();
    listeningWindowDuration2 = par("listeningWindowDuration2").intValue();
    listeningWindowRX1 = par("listeningWindowRX1").doubleValue();
    listeningWindowRX2 = par("listeningWindowRX2").doubleValue();
    listeningWindowRXR = par("listeningWindowRXR").doubleValue();
    listeningWindowRXC = par("listeningWindowRXC").doubleValue();
    listeningWindowRXCR = par("listeningWindowRXCR").doubleValue();
    receiveDelay1 = par("receiveDelay1").doubleValue();
    joinAcceptDelay1 = par("joinAcceptDelay1").doubleValue();
    ts011RxrDelay = par("ts011RxrDelay").doubleValue();
    epRxrDelay = par("epRxrDelay").doubleValue();

    ts011DefaultWorChannel = par("ts011DefaultWorChannel").intValue();
    ts011SecondWorChannel = par("ts011SecondWorChannel").intValue();
    ts011SecondChannelEnabled = par("ts011SecondChannelEnabled").boolValue();
    ts011Rx2Channel = par("ts011Rx2Channel").intValue();
    ts011RxrChannel = par("ts011RxrChannel").intValue();
    ts011CadPeriodicity = par("ts011CadPeriodicity").doubleValue();
    ts011CadToRxSymbols = par("ts011CadToRxSymbols").intValue();
    ts011ChannelScanSymbols = par("ts011ChannelScanSymbols").intValue();
    ts011WorAckDelay = par("ts011WorAckDelay").doubleValue();
    ts011WorDataDelay = par("ts011WorDataDelay").doubleValue();
    ts011WorAckWindow = par("ts011WorAckWindow").doubleValue();
    ts011UplinkListenTimeout = par("ts011UplinkListenTimeout").doubleValue();
    if (ts011CadPeriodicity <= 0 || ts011ChannelScanSymbols <= 0)
        throw cRuntimeError("TS011 CAD periodicity and channel scan length must be positive");
    if (ts011DefaultWorChannel < 0 || ts011DefaultWorChannel > 255 ||
        ts011SecondWorChannel < 0 || ts011SecondWorChannel > 255 ||
        (ts011SecondChannelEnabled && ts011DefaultWorChannel == ts011SecondWorChannel))
        throw cRuntimeError("WOR channels must be distinct logical frequency IDs in [0,255]");

    currentConsumptionRx = par("currentConsumptionRx").doubleValue();
    currentConsumptionTx = par("currentConsumptionTx").doubleValue();
    currentConsumptionIdle = par("currentConsumptionIdle").doubleValue();
    currentConsumptionWOR = par("currentConsumptionWOR").doubleValue();
    currentConsumptionBaseOffset = par("currentConsumptionBaseOffset").doubleValue();
    lteAttachEnergyMAs = par("lteAttachEnergyMAs").doubleValue();
    lteTransmitEnergyMAs = par("lteTransmitEnergyMAs").doubleValue();

    if (deviceType == DevType::RELAY && retransmissionModel == RetransmissionModel::TS011 &&
        par("ts011DynamicWorEnergy").boolValue()) {
        try {
            cadDurationSeconds = sx126xCadDurationSeconds(
                par("ts011EndDeviceSpreadingFactor").intValue(), par("ts011CadSymbolNum").intValue());
            const int channels = ts011SecondChannelEnabled ? 2 : 1;
            currentConsumptionWOR = worStandbyCurrentMa(ts011CadPeriodicity, cadDurationSeconds,
                channels, par("currentConsumptionCad").doubleValue(), currentConsumptionIdle);
            cadDutyFraction = channels * cadDurationSeconds / ts011CadPeriodicity;
        }
        catch (const std::invalid_argument& error) {
            throw cRuntimeError("%s", error.what());
        }
    }

    radioRXTimer = new cMessage("classAReceiveTimer");
    radioRXTimer->setSchedulingPriority(-10); // Open before a same-timestamp preamble arrives.
    radioRXTimer->addPar("windowIndex") = 0;
    radioActivity = RadioActivity::UNKNOWN;
    updateRadioActivity(RadioActivity::RADIOOFF);
    rssiOfCurrentReceive = sensitivity - 1;
    lastSpreadingFactor = 12;
    wakeupRelay = registerSignal("wakeupRelay");

    if (deviceType == DevType::RELAY || deviceType == DevType::IPBRIDGE)
        getSimulation()->getSystemModule()->subscribe("wakeupRelay", this);

    if (deviceType == DevType::ENDNODE) {
        timerListeningWindowStart1 = new cMessage("dailyRxStart1", SelfMsgType::WINDOWTIMER);
        timerListeningWindowStop1 = new cMessage("dailyRxStop1", SelfMsgType::WINDOWTIMER);
        timerListeningWindowStart2 = new cMessage("dailyRxStart2", SelfMsgType::WINDOWTIMER);
        timerListeningWindowStop2 = new cMessage("dailyRxStop2", SelfMsgType::WINDOWTIMER);
        scheduleAt(simTime() + listeningWindowStart1 * 60, timerListeningWindowStart1);
        scheduleAt(simTime() + (listeningWindowStart1 + listeningWindowDuration1) * 60,
                   timerListeningWindowStop1);
        scheduleAt(simTime() + listeningWindowStart2 * 60, timerListeningWindowStart2);
        scheduleAt(simTime() + (listeningWindowStart2 + listeningWindowDuration2) * 60,
                   timerListeningWindowStop2);
    }

    WATCH(radioActivity);
    WATCH(collisionCount);
    WATCH(energyConsumptionRx);
    WATCH(energyConsumptionTx);
    WATCH(energyConsumptionIdle);
    WATCH(energyConsumptionWOR);
    WATCH(energyConsumptionBaseOffset);
    WATCH(activeReceiveChannel);

    scheduleAt(simTime(), new cMessage("radioInit", SelfMsgType::RADIOINIT));
}

void Radio::handleMessage(cMessage *msg)
{
    updateDaytime();
    if (msg->isSelfMessage()) {
        switch (msg->getKind()) {
            case SelfMsgType::RADIOINIT:
                updateRadio();
                delete msg;
                return;
            case SelfMsgType::WINDOWTIMER:
                scheduleAt(simTime() + 24 * 3600, msg);
                updateRadio();
                return;
            case SelfMsgType::WOR_CAD_ACQUIRE:
                handleWorAcquisition(msg);
                return;
            case SelfMsgType::TXTIMER: {
                const int txKind = msg->par("txKind").longValue();
                const int txChannel = msg->par("txChannel").longValue();
                const bool ackExpected = msg->par("worAckExpected").boolValue();
                const bool includeRxr = msg->par("includeRxr").boolValue();
                getParentModule()->getDisplayString().setTagArg("r", 0, "0");
                updateIdleRadioState();
                if (txKind == MsgType::MSGWOR) {
                    if (ackExpected) {
                        auto *open = new cMessage("worAckRxOpen", SelfMsgType::WORACK_RX_OPEN);
                        open->setSchedulingPriority(-10);
                        open->addPar("channel") = txChannel;
                        scheduleAt(simTime() + ts011WorAckDelay, open);
                    }
                }
                else if (deviceType != DevType::GATEWAY && msg->par("isUplink").boolValue() && txKind != MsgType::MSGWORACK &&
                         !disableClassAWindows) {
                    activeReceiveChannel = txChannel;
                    scheduleClassAReception(includeRxr, txKind == MsgType::JOINREQUEST,
                        msg->par("DeviceId").longValue(),
                        msg->par("PacketId").longValue(), msg->par("AttemptId").longValue(), txChannel);
                    if (deviceType == DevType::ENDNODE) {
                        auto *complete = new cMessage("MAC transmission complete", MsgType::MAC_TX_COMPLETE);
                        complete->addPar("PacketId") = msg->par("PacketId").longValue();
                        complete->addPar("AttemptId") = msg->par("AttemptId").longValue();
                        complete->addPar("sf") = msg->par("sf").longValue();
                        send(complete, "toLogic");
                    }
                }
                disableClassAWindows = false;
                delete msg;
                if (!classAInProgress && !sendQueue.empty()) sendToNetwork(nullptr);
                return;
            }
            case SelfMsgType::WORACK_RX_OPEN: {
                if ((radioActivity == RadioActivity::RECEIVING || radioActivity == RadioActivity::TRANSMITTING) &&
                    simTime() < busyUntil) {
                    delete msg; // A receive window cannot create a second RF chain.
                    return;
                }
                updateRadioActivity(RadioActivity::LISTENING);
                activeReceiveChannel = msg->hasPar("channel") ? msg->par("channel").longValue() : activeReceiveChannel;
                auto *close = new cMessage("worAckRxClose", SelfMsgType::WORACK_RX_CLOSE);
                scheduleAt(simTime() + ts011WorAckWindow, close);
                delete msg;
                return;
            }
            case SelfMsgType::WORACK_RX_CLOSE:
                updateRadio();
                delete msg;
                return;
            case SelfMsgType::RXC:
                openClassAWindow(msg->par("windowIndex").longValue(), msg);
                return;
            case SelfMsgType::RX1:
            case SelfMsgType::RX2:
            case SelfMsgType::RXR:
                if (radioActivity == RadioActivity::RECEIVING && currentRcvMessage != nullptr) {
                    scheduleAt(std::max(simTime(), busyUntil) + SimTime(1, SIMTIME_MS), msg);
                    return;
                }
                classAWindowOpen = 0;
                if (downlinkReceivedInClassA || msg->getKind() == SelfMsgType::RXR ||
                    (msg->getKind() == SelfMsgType::RX2 && !rxrPending)) {
                    finishClassAReception();
                }
                else {
                    scheduleNextClassAWindow(msg->getKind() == SelfMsgType::RX1 ? 2 : 3);
                }
                updateRadio();
                if (!classAInProgress && !sendQueue.empty())
                    sendToNetwork(nullptr);
                return;
            case SelfMsgType::RELAY_RX_TIMEOUT:
                updateRadio();
                delete msg;
                return;
            case SelfMsgType::RXTIMER:
                if (currentRcvMessage != nullptr) {
                    if (deviceType == DevType::RELAY && currentRcvMessage->isUplink &&
                        !currentRcvMessage->isWOR())
                        accessGrants.erase(currentRcvMessage->senderId);
                    if (deviceType == DevType::IPBRIDGE && currentRcvMessage->isUplink &&
                        currentRcvMessage->relayRequired) {
                        energyConsumptionLteAttach += lteAttachEnergyMAs;
                        energyConsumptionLteTransmit += lteTransmitEnergyMAs;
                        ++lteAttachCount;
                        ++lteTransmitCount;
                    }
                    if (!currentRcvMessage->isUplink && !currentRcvMessage->isWorAck() &&
                        classAResponseMatches(currentRcvMessage)) {
                        if (currentRcvMessage->hasPar("ReceiveWindow"))
                            currentRcvMessage->par("ReceiveWindow") = classAWindowOpen;
                        else currentRcvMessage->addPar("ReceiveWindow") = classAWindowOpen;
                        downlinkReceivedInClassA = true;
                        finishClassAReception();
                    }
                    send(currentRcvMessage, "toLogic");
                    currentRcvMessage = nullptr;
                }
                radioUpdateReceive = nullptr;
                rssiOfCurrentReceive = sensitivity - 1;
                if (!sendQueue.empty() && !classAInProgress)
                    sendToNetwork(nullptr);
                else
                    updateRadio();
                delete msg;
                return;
            default:
                delete msg;
                return;
        }
    }

    if (msg->arrivedOn("fromLogic")) {
        if (msg->getKind() == MsgType::RELAY_ACCESS_GRANT) {
            accessGrants[msg->par("SenderId").longValue()] = {
                static_cast<int>(msg->par("PacketId").longValue()),
                static_cast<int>(msg->par("AttemptId").longValue()),
                static_cast<int>(msg->par("channel").longValue()),
                SimTime(msg->par("until").doubleValue())};
            const auto until = SimTime(msg->par("until").doubleValue());
            scheduleAt(until, new cMessage("relay grant expires", SelfMsgType::RELAY_RX_TIMEOUT));
            updateRadio();
            delete msg;
            return;
        }
        if (msg->getKind() == MsgType::RESET_ADR) {
            if (lastSpreadingFactor < 12)
                ++lastSpreadingFactor;
            delete msg;
            return;
        }
        if (msg->getKind() == MsgType::ADR_INCREASE_SF) {
            if (lastSpreadingFactor < 12)
                ++lastSpreadingFactor;
            delete msg;
            return;
        }
        if (msg->getKind() == MsgType::ADR_DECREASE_SF) {
            if (lastSpreadingFactor > 7)
                --lastSpreadingFactor;
            delete msg;
            return;
        }
        auto *packet = dynamic_cast<LoRaPacket *>(msg);
        if (packet == nullptr) {
            delete msg;
            return;
        }
        if (packet->getKind() == MsgType::JOINREQUEST)
            lastSpreadingFactor = 12;
        sendToNetwork(packet);
        return;
    }

    if (msg->arrivedOn("radioIn")) {
        auto *packet = dynamic_cast<LoRaPacket *>(msg);
        if (packet != nullptr)
            receiveFromNetwork(packet);
        else
            delete msg;
        return;
    }

    delete msg;
}

void Radio::openClassAWindow(int index, cMessage *timer)
{
    if (!classAInProgress || downlinkReceivedInClassA)
        return;
    const double first = classAJoin ? joinAcceptDelay1 : receiveDelay1;
    const double delay = index == 3 ? (retransmissionModel == RetransmissionModel::TS011 ?
        ts011RxrDelay : epRxrDelay) : first + index - 1;
    const double duration = index == 1 ? listeningWindowRX1 : index == 2 ? listeningWindowRX2 : listeningWindowRXR;
    const auto nominalEnd = classATxEnd + delay + duration;
    if (simTime() >= nominalEnd) {
        // A busy radio does not move the receive window to a later time.
        if (index == 3 || (index == 2 && !rxrPending)) finishClassAReception();
        else scheduleNextClassAWindow(index + 1);
        updateRadio();
        if (!classAInProgress && !sendQueue.empty()) sendToNetwork(nullptr);
        return;
    }
    if ((radioActivity == RadioActivity::RECEIVING || radioActivity == RadioActivity::TRANSMITTING) &&
        simTime() < busyUntil) {
        // Do not overwrite an in-progress demodulation or transmission.
        scheduleAt(std::min(nominalEnd, busyUntil + SimTime(1, SIMTIME_MS)), timer);
        return;
    }
    classAWindowOpen = index;
    updateRadioActivity(RadioActivity::LISTENING);
    timer->setKind(index == 1 ? SelfMsgType::RX1 : index == 2 ? SelfMsgType::RX2 : SelfMsgType::RXR);
    if (index == 1)
        activeReceiveChannel = classARx1Channel;
    else if (index == 2)
        activeReceiveChannel = ts011Rx2Channel;
    else if (index == 3)
        activeReceiveChannel = ts011RxrChannel;
    scheduleAt(nominalEnd, timer);
}

void Radio::scheduleClassAReception(bool includeRxr, bool join, int deviceId, int packetId, int attemptId, int channel)
{
    if (radioRXTimer->isScheduled())
        cancelEvent(radioRXTimer);
    rxrPending = includeRxr;
    classAInProgress = true;
    classAJoin = join;
    classATxEnd = simTime();
    classAPacketId = packetId;
    classADeviceId = deviceId;
    classAAttemptId = attemptId;
    classARx1Channel = channel;
    classAWindowOpen = 0;
    downlinkReceivedInClassA = false;
    scheduleNextClassAWindow(1);
}

void Radio::scheduleNextClassAWindow(int index)
{
    radioRXTimer->setKind(SelfMsgType::RXC);
    radioRXTimer->par("windowIndex") = index;
    const double first = classAJoin ? joinAcceptDelay1 : receiveDelay1;
    const double delay = index == 3 ? (retransmissionModel == RetransmissionModel::TS011 ?
        ts011RxrDelay : epRxrDelay) : first + index - 1;
    scheduleAt(std::max(simTime(), classATxEnd + delay), radioRXTimer);
}

void Radio::finishClassAReception()
{
    if (radioRXTimer->isScheduled()) cancelEvent(radioRXTimer);
    classAInProgress = false;
    classAWindowOpen = 0;
    rxrPending = false;
}

bool Radio::classAResponseMatches(const LoRaPacket *packet) const
{
    if (!classAInProgress || classAWindowOpen == 0 || !packet->hasPar("KeeperIndex") || !packet->hasPar("PacketId") ||
        !packet->hasPar("AttemptId")) return false;
    const int kind = packet->encapsulated ? packet->originalKind : packet->getKind();
    // A relay's forwarded uplink is a data uplink even when it encapsulates Join.
    const bool validKind = deviceType == DevType::RELAY ||
        (classAJoin ? kind == MsgType::JOINACCEPT : kind == MsgType::ACK || kind == MsgType::DOWNLINK);
    // OMNeT++ cMessage exposes no const accessor for message parameters.
    auto *metadata = const_cast<LoRaPacket *>(packet);
    return validKind && metadata->par("KeeperIndex").longValue() == classADeviceId &&
        metadata->par("PacketId").longValue() == classAPacketId &&
        metadata->par("AttemptId").longValue() == classAAttemptId;
}

void Radio::receiveSignal(cComponent *, simsignal_t id, double value, cObject *)
{
    if (id != wakeupRelay || retransmissionModel != RetransmissionModel::EP431 ||
        (deviceType != DevType::RELAY && deviceType != DevType::IPBRIDGE))
        return;
    Enter_Method("receiveSignal");
    relayListeningUntil = std::max(relayListeningUntil, simTime() + value);
    scheduleAt(relayListeningUntil, new cMessage("relayListenTimeout", SelfMsgType::RELAY_RX_TIMEOUT));
    if (radioActivity != RadioActivity::RECEIVING && radioActivity != RadioActivity::TRANSMITTING)
        updateRadioActivity(RadioActivity::LISTENING);
}

void Radio::updateRadio()
{
    if ((radioActivity == RadioActivity::RECEIVING || radioActivity == RadioActivity::TRANSMITTING) &&
        simTime() < busyUntil)
        return;
    updateIdleRadioState();
}

bool Radio::roleAcceptsPacket(const LoRaPacket *packet) const
{
    return radioActivity == RadioActivity::LISTENING && packet->channel == activeReceiveChannel;
}

bool Radio::acceptsAirDelivery(const LoRaPacket *packet) const
{
    if (packet == nullptr)
        return false;
    // Keep a WOR preamble available for a later scan, even if this radio is
    // currently busy on another frequency. This is NOT parallel reception.
    if (deviceType == DevType::RELAY && retransmissionModel == RetransmissionModel::TS011 &&
        packet->isWOR() && packet->worValid && isConfiguredWorChannel(packet->channel))
        return true;
    if (radioActivity == RadioActivity::TRANSMITTING)
        return false; // Half duplex: missed reception, not an RF collision.
    if (radioActivity == RadioActivity::RECEIVING)
        return packet->channel == activeReceiveChannel;
    return roleAcceptsPacket(packet);
}

bool Radio::isConfiguredWorChannel(int channel) const
{
    return channel == ts011DefaultWorChannel ||
        (ts011SecondChannelEnabled && channel == ts011SecondWorChannel);
}

bool Radio::relayScanOverlaps(const LoRaPacket *packet, simtime_t *scanTime, simtime_t earliest) const
{
    if (!packet->isWOR() || ts011CadPeriodicity <= 0)
        return false;
    double offset = 0;
    if (packet->channel == ts011SecondWorChannel) {
        if (!ts011SecondChannelEnabled)
            return false;
        offset = ts011CadPeriodicity / 2.0;
    }
    else if (packet->channel != ts011DefaultWorChannel) {
        return false;
    }

    auto *metadata = const_cast<LoRaPacket *>(packet);
    const double start = packet->hasPar("WorAirStart") ? metadata->par("WorAirStart").doubleValue() : simTime().dbl();
    const double symbol = packet->symbolTime();
    // Point-CAD abstraction: acquire at the actual grid event, never at the
    // beginning of a preamble whose matching scan is still in the future.
    const double earliestScan = std::max(start, earliest.dbl());
    const double latestScan = start + packet->calcMsgPreamble() * symbol -
                              (6 + ts011CadToRxSymbols) * symbol;
    const double n = std::ceil((earliestScan - offset) / ts011CadPeriodicity - 1e-12);
    const double candidate = SimTime(offset + std::max(0.0, n) * ts011CadPeriodicity).dbl();
    if (candidate + 1e-12 < earliestScan || candidate - 1e-12 > latestScan)
        return false;
    if (scanTime != nullptr)
        *scanTime = SimTime(candidate);
    return true;
}

void Radio::scheduleWorAcquisition(LoRaPacket *packet, simtime_t earliest)
{
    simtime_t scan;
    if (!relayScanOverlaps(packet, &scan, earliest)) {
        delete packet;
        return;
    }
    auto *timer = new cMessage("WOR CAD acquisition", SelfMsgType::WOR_CAD_ACQUIRE);
    timer->setSchedulingPriority(5); // Air arrivals and completed RX/TX precede CAD.
    pendingWorAcquisitions.emplace(timer, packet);
    scheduleAt(scan, timer);
}

void Radio::handleWorAcquisition(cMessage *timer)
{
    auto found = pendingWorAcquisitions.find(timer);
    ASSERT(found != pendingWorAcquisitions.end());
    auto *packet = found->second;
    pendingWorAcquisitions.erase(found);
    delete timer;
    if (radioActivity == RadioActivity::WOR ||
        (radioActivity == RadioActivity::RECEIVING && activeReceiveChannel == packet->channel)) {
        receiveFromNetwork(packet, true);
    }
    else {
        ++cadBusySkips;
        // Preserve the scan phase after a busy interval. Long preambles may
        // still be acquired on a later scan; short ones simply expire.
        scheduleWorAcquisition(packet, simTime() + SimTime(ts011CadPeriodicity));
    }
}

void Radio::receiveFromNetwork(LoRaPacket *msg, bool fromCad)
{
    if (msg->senderId != myKeeperIndex && deviceType == DevType::ENDNODE) {
        delete msg;
        return;
    }
    if (deviceType == DevType::RELAY && retransmissionModel == RetransmissionModel::TS011 &&
        msg->isWOR() && !fromCad) {
        if (!msg->hasPar("WorAirStart")) msg->addPar("WorAirStart") = simTime().dbl();
        // An on-channel signal immediately participates in collision/capture.
        // Otherwise only an actual CAD event may start WOR reception.
        if (radioActivity != RadioActivity::RECEIVING || activeReceiveChannel != msg->channel) {
            scheduleWorAcquisition(msg, simTime());
            return;
        }
    }
    if (!acceptsAirDelivery(msg)) {
        delete msg;
        return;
    }

    const bool relayAccessUplink = deviceType == DevType::RELAY && msg->isUplink &&
        msg->relayRequired && !msg->isRelayed;
    const bool relayAccessDownlink = deviceType == DevType::ENDNODE && !msg->isUplink && msg->isRelayed;
    const bool relayBackhaulUplink = deviceType == DevType::GATEWAY && msg->isUplink && msg->isRelayed;
    const bool relayBackhaulDownlink = deviceType == DevType::RELAY && !msg->isUplink && msg->relayRequired;
    const bool ipBridgeAccessUplink = deviceType == DevType::IPBRIDGE && msg->isUplink && msg->relayRequired;
    const bool relayAccessLink = relayAccessUplink || relayAccessDownlink;
    const bool reliableRelayBackhaul = relayBackhaulUplink || relayBackhaulDownlink;

    if (relayAccessUplink || ipBridgeAccessUplink) {
        const DevType targetType = relayAccessUplink ? DevType::RELAY : DevType::IPBRIDGE;
        Radio *closest = nullptr;
        double best = std::numeric_limits<double>::max();
        for (const Node &node : keeper->returnRadioDevices()) {
            if (node.devType != targetType || node.radio == nullptr)
                continue;
            const double dx = node.radio->position.x - msg->senderPositionX;
            const double dy = node.radio->position.y - msg->senderPositionY;
            const double d2 = dx * dx + dy * dy;
            if (d2 < best) {
                best = d2;
                closest = node.radio;
            }
        }
        if (closest != this) {
            delete msg;
            return;
        }
    }

    if (radioActivity == RadioActivity::TRANSMITTING) {
        keeper->recordColision(myKeeperIndex, msg->getSenderModuleId());
        dropByCollision(msg);
        return;
    }

    double rssi = getRssi(msg);
    const double realSensitivity = sensitivity + std::sqrt(keeper->distortionLevel) +
                                   2.5 * (12 - msg->spreadingFactor);
    const double randomPathLoss = normal(0, par("pathlosStd"));
    rssi += randomPathLoss;
    const double relayAccessLinkMarginDb = par("relayAccessLinkMarginDb").doubleValue();
    if (relayAccessLink)
        rssi += relayAccessLinkMarginDb;
    if (ipBridgeAccessUplink)
        rssi += par("ipBridgeLinkMarginDb").doubleValue();
    if (reliableRelayBackhaul)
        rssi += par("relayBackhaulLosGainDb").doubleValue();
    msg->rssi = rssi;
    msg->rssi_margin = rssi - realSensitivity;
    msg->snr = std::max(-20.0, std::min(11.0, msg->rssi_margin - 6.0));

    bool receiveOk = false;
    if (radioActivity == RadioActivity::RECEIVING) {
        if (rssi > rssiOfCurrentReceive + 6) {
            if (radioUpdateReceive != nullptr && radioUpdateReceive->isScheduled())
                cancelAndDelete(radioUpdateReceive);
            radioUpdateReceive = nullptr;
            dropByCollision(currentRcvMessage);
            currentRcvMessage = nullptr;
            receiveOk = true;
        }
        else if (rssi < rssiOfCurrentReceive - 6) {
            dropByCollision(msg);
            return;
        }
        else {
            if (radioUpdateReceive != nullptr && radioUpdateReceive->isScheduled())
                cancelAndDelete(radioUpdateReceive);
            radioUpdateReceive = nullptr;
            dropByCollision(currentRcvMessage);
            currentRcvMessage = nullptr;
            dropByCollision(msg);
            busyUntil = simTime(); // Both receptions ended; do not leave a timerless busy radio.
            updateRadio();
            return;
        }
    }
    else if (radioActivity == RadioActivity::LISTENING) {
        receiveOk = rssi > realSensitivity;
    }
    else if ((radioActivity == RadioActivity::WOR || radioActivity == RadioActivity::RADIOOFF) &&
             deviceType == DevType::RELAY && retransmissionModel == RetransmissionModel::TS011 &&
             msg->isWOR()) {
        receiveOk = fromCad && rssi > realSensitivity && msg->worValid;
    }

    keeper->recordFrameReception(myKeeperIndex, msg->getSenderModuleId(), rssi, receiveOk);
    if (!receiveOk) {
        delete msg;
        return;
    }

    if (deviceType == DevType::RELAY && msg->isWOR()) {
        const double airStart = msg->par("WorAirStart").doubleValue();
        const double preambleEnd = airStart + msg->calcMsgPreamble() * msg->symbolTime();
        msg->tOffsetMs = static_cast<int>(std::ceil((preambleEnd - simTime().dbl()) * 1000.0));
        if (msg->channel == ts011DefaultWorChannel) ++cadAcquisitionsDefault;
        else ++cadAcquisitionsSecond;
    }

    const double duration = msg->calcMsgDuration();
    activeReceiveChannel = msg->channel;
    updateRadioActivity(RadioActivity::RECEIVING);
    busyUntil = msg->isWOR() && msg->hasPar("WorAirStart") ?
        SimTime(msg->par("WorAirStart").doubleValue()) + SimTime(duration) : simTime() + duration;
    rssiOfCurrentReceive = rssi;
    currentRcvMessage = msg;
    radioUpdateReceive = new cMessage("radioReceiveComplete", SelfMsgType::RXTIMER);
    scheduleAt(busyUntil, radioUpdateReceive);
}

void Radio::dropByCollision(LoRaPacket *packet)
{
    if (packet == nullptr)
        return;
    packet->autodetectUplink();
    if (packet->hasPar("KeeperIndex") && packet->hasPar("PacketId")) {
        const long dev = packet->par("KeeperIndex").longValue();
        const long id = packet->par("PacketId").longValue();
        if (packet->isUplink)
            keeper->reportUplinkCollision(dev, id);
        else
            keeper->reportDownlinkCollision(dev, id);
    }
    ++collisionCount;
    delete packet;
}

void Radio::sendToNetwork(LoRaPacket *msg)
{
    if (msg == nullptr) {
        while (!sendQueue.empty()) {
            auto *next = new LoRaPacket(sendQueue.front());
            sendQueue.pop_front();
            if (next->hasPar("TxDeadline") && simTime().dbl() > next->par("TxDeadline").doubleValue()) {
                ++lateDownlinkDrops;
                delete next;
                continue;
            }
            sendToNetwork(next);
            break;
        }
        return;
    }
    if (msg->hasPar("TxDeadline") && simTime().dbl() > msg->par("TxDeadline").doubleValue()) {
        ++lateDownlinkDrops;
        delete msg;
        return;
    }
    const bool urgentControl = msg->isWorAck() || (!msg->isUplink && msg->hasPar("TxDeadline"));
    if (classAInProgress && !urgentControl) {
        sendQueue.push_back(*msg);
        delete msg;
        return;
    }
    if ((radioActivity == RadioActivity::TRANSMITTING || radioActivity == RadioActivity::RECEIVING) &&
        simTime() < busyUntil) {
        sendQueue.push_back(*msg);
        delete msg;
        return;
    }
    directSendWithDelete(msg);
}

void Radio::directSendWithDelete(LoRaPacket *msg)
{
    if (msg->hasPar("TxDeadline") && simTime().dbl() > msg->par("TxDeadline").doubleValue()) {
        ++lateDownlinkDrops;
        delete msg;
        if (!sendQueue.empty() && radioActivity != RadioActivity::TRANSMITTING &&
            radioActivity != RadioActivity::RECEIVING) sendToNetwork(nullptr);
        return;
    }
    if (deviceType == DevType::ENDNODE)
        msg->senderId = myKeeperIndex;
    if (deviceType == DevType::RELAY && msg->encapsulated && msg->isUplink)
        msg->forwardingRelayId = myKeeperIndex;
    msg->senderPositionX = position.x;
    msg->senderPositionY = position.y;
    msg->transmissionPower = txPower;
    if (msg->spreadingFactor == -1)
        msg->spreadingFactor = lastSpreadingFactor;

    activeReceiveChannel = msg->channel;
    updateRadioActivity(RadioActivity::TRANSMITTING);
    activeTxIsWor = msg->isWOR();
    const double duration = msg->calcMsgDuration();
    busyUntil = simTime() + duration;
    auto *done = new cMessage("radioTransmitComplete", SelfMsgType::TXTIMER);
    done->addPar("txKind") = msg->getKind();
    done->addPar("txChannel") = msg->channel;
    done->addPar("worAckExpected") = msg->isWOR() && msg->worType == WorType::CLASS_A_UPLINK;
    done->addPar("includeRxr") = msg->rxrEligible;
    done->addPar("isUplink") = msg->isUplink;
    done->addPar("PacketId") = msg->hasPar("PacketId") ? msg->par("PacketId").longValue() : -1L;
    done->addPar("DeviceId") = msg->hasPar("KeeperIndex") ? msg->par("KeeperIndex").longValue() : -1L;
    done->addPar("AttemptId") = msg->hasPar("AttemptId") ? msg->par("AttemptId").longValue() : 0L;
    done->addPar("sf") = static_cast<long>(msg->spreadingFactor);
    if (msg->hasPar("RadioTxEnd")) msg->par("RadioTxEnd") = busyUntil.dbl();
    else msg->addPar("RadioTxEnd") = busyUntil.dbl();
    scheduleAt(busyUntil, done);
    if (msg->isWOR())
        disableClassAWindows = true;
    getParentModule()->getDisplayString().setTagArg("r", 0, "200");

    for (const Node &device : keeper->returnRadioDevices()) {
        if (device.radio == nullptr || device.radio == this)
            continue;
        if (!device.radio->acceptsAirDelivery(msg))
            continue;
        sendDirect(msg->dupLoRa(), device.radio->getParentModule(), "radioIn");
    }
    keeper->recordFrameTransmission(myKeeperIndex, duration);
    // Diagnostic only: count actual on-air forwarding, not an ED's intention
    // to use a relay and not a packet merely admitted into the relay queue.
    if (deviceType == DevType::RELAY && msg->isUplink && msg->isRelayed &&
        msg->hasPar("KeeperIndex") && msg->hasPar("PacketId"))
        keeper->recordActualRelayUplink(msg->par("KeeperIndex").longValue(),
                                       msg->par("PacketId").longValue(), myKeeperIndex);
    delete msg;
}

bool Radio::isListening() const { return radioActivity == RadioActivity::LISTENING; }
bool Radio::isOffOrUnknown() const noexcept
{
    return radioActivity == RadioActivity::RADIOOFF || radioActivity == RadioActivity::UNKNOWN;
}

double Radio::getRssi(LoRaPacket *msg)
{
    const double dx = position.x - msg->senderPositionX;
    const double dy = position.y - msg->senderPositionY;
    return calculateReceivedPower(msg->transmissionPower, std::sqrt(dx * dx + dy * dy));
}

double Radio::calculateReceivedPower(float pSend, double distance)
{
    if (static_cast<int>(distance) == 0)
        return pSend;
    const double pathLoss = 127.5 + 35.2 * std::log10(distance / 500.0);
    return std::min(static_cast<double>(pSend), pSend - pathLoss);
}

void Radio::updateDaytime()
{
    dayTime = static_cast<int>((simTime().inUnit(SIMTIME_S) / 60) % (60 * 24));
}

void Radio::updateRadioActivity(RadioActivity::ra next)
{
    const double duration = (simTime() - lastRadioActivityChange).dbl();
    energyConsumptionBaseOffset += duration * currentConsumptionBaseOffset;
    switch (radioActivity) {
        case RadioActivity::RADIOOFF:
        case RadioActivity::TIMER:
            energyConsumptionIdle += duration * currentConsumptionIdle;
            break;
        case RadioActivity::RECEIVING:
        case RadioActivity::LISTENING:
            energyConsumptionRx += duration * currentConsumptionRx;
            break;
        case RadioActivity::WOR:
        case RadioActivity::EWOR:
            energyConsumptionWOR += duration * currentConsumptionWOR;
            worStateSeconds += duration;
            break;
        case RadioActivity::TRANSMITTING:
            energyConsumptionTx += duration * currentConsumptionTx;
            // Subset of Tx, never added to the total a second time.
            // Integrates actual time, including a transmission cut by sim-time-limit.
            if (activeTxIsWor) {
                actualWorTxSeconds += duration;
                actualWorTxChargeMAs += duration * currentConsumptionTx;
            }
            break;
        default:
            break;
    }
    radioActivity = next;
    lastRadioActivityChange = simTime();
    keeper->recordEnergyConsumption(myKeeperIndex,
        energyConsumptionIdle + energyConsumptionRx + energyConsumptionTx + energyConsumptionWOR +
        energyConsumptionBaseOffset + energyConsumptionLteAttach + energyConsumptionLteTransmit);
}

void GatewayRadio::updateIdleRadioState()
{
    updateRadioActivity(RadioActivity::LISTENING);
}

bool GatewayRadio::roleAcceptsPacket(const LoRaPacket *packet) const
{
    return packet->isUplink && !packet->isWOR();
}

void EndDeviceRadio::updateIdleRadioState()
{
    const bool dailyWindow =
        (dayTime >= listeningWindowStart1 && dayTime < listeningWindowStart1 + listeningWindowDuration1) ||
        (dayTime >= listeningWindowStart2 && dayTime < listeningWindowStart2 + listeningWindowDuration2);
    updateRadioActivity(dailyWindow || classAWindowOpen != 0 ? RadioActivity::LISTENING : RadioActivity::RADIOOFF);
}

bool EndDeviceRadio::roleAcceptsPacket(const LoRaPacket *packet) const
{
    return !packet->encapsulated && !packet->isUplink && packet->channel == activeReceiveChannel &&
           (radioActivity == RadioActivity::LISTENING || radioActivity == RadioActivity::RECEIVING);
}

void RelayRadio::updateIdleRadioState()
{
    if (retransmissionModel == RetransmissionModel::TS011) {
        for (auto it = accessGrants.begin(); it != accessGrants.end();) {
            if (it->second.until <= simTime()) it = accessGrants.erase(it);
            else ++it;
        }
        if (classAWindowOpen != 0) {
            updateRadioActivity(RadioActivity::LISTENING);
        }
        else if (!accessGrants.empty()) {
            activeReceiveChannel = accessGrants.begin()->second.channel;
            updateRadioActivity(RadioActivity::LISTENING);
        }
        else {
            updateRadioActivity(RadioActivity::WOR);
        }
    }
    else if (classAWindowOpen != 0 || simTime() < relayListeningUntil) {
        updateRadioActivity(RadioActivity::LISTENING);
    }
    else {
        updateRadioActivity(RadioActivity::RADIOOFF);
    }
}

bool RelayRadio::roleAcceptsPacket(const LoRaPacket *packet) const
{
    if (packet->isUplink && packet->isRelayed)
        return false; // TS011 is strictly single-hop in this simulator.
    if (retransmissionModel == RetransmissionModel::TS011 && packet->isWOR())
        return radioActivity == RadioActivity::WOR && packet->worValid && relayScanOverlaps(packet);
    if (retransmissionModel == RetransmissionModel::TS011 && packet->isUplink && packet->relayRequired) {
        const auto found = accessGrants.find(packet->senderId);
        auto *metadata = const_cast<LoRaPacket *>(packet);
        return radioActivity == RadioActivity::LISTENING && found != accessGrants.end() && simTime() < found->second.until &&
            packet->channel == activeReceiveChannel &&
            packet->channel == found->second.channel && packet->hasPar("PacketId") && packet->hasPar("AttemptId") &&
            metadata->par("PacketId").longValue() == found->second.packetId &&
            metadata->par("AttemptId").longValue() == found->second.attemptId;
    }
    if (!packet->isUplink && packet->encapsulated && packet->forwardingRelayId != myKeeperIndex)
        return false;
    return radioActivity == RadioActivity::LISTENING && packet->channel == activeReceiveChannel;
}

void IpBridgeRadio::updateIdleRadioState()
{
    updateRadioActivity(simTime() < relayListeningUntil ? RadioActivity::LISTENING : RadioActivity::RADIOOFF);
}

bool IpBridgeRadio::roleAcceptsPacket(const LoRaPacket *packet) const
{
    return packet->isUplink && packet->relayRequired && !packet->isRelayed &&
           radioActivity == RadioActivity::LISTENING;
}

void Radio::finish()
{
    updateRadioActivity(radioActivity);
    recordScalar("Energy consumption total Tx", energyConsumptionTx);
    recordScalar("Energy consumption total Rx", energyConsumptionRx);
    recordScalar("Energy consumption total Idle", energyConsumptionIdle);
    recordScalar("Energy consumption total WOR", energyConsumptionWOR);
    recordScalar("Energy consumption total BaseOffset", energyConsumptionBaseOffset);
    recordScalar("Energy consumption total LTE attach", energyConsumptionLteAttach);
    recordScalar("Energy consumption total LTE transmit", energyConsumptionLteTransmit);
    recordScalar("WOR actual Tx duration s", actualWorTxSeconds);
    recordScalar("WOR actual Tx charge mAs", actualWorTxChargeMAs);
    recordScalar("LTE attach count", lteAttachCount);
    recordScalar("LTE transmit count", lteTransmitCount);
    recordScalar("Radio collision count", collisionCount);
    recordScalar("Radio expired transmission drops", lateDownlinkDrops);
    if (deviceType == DevType::RELAY && retransmissionModel == RetransmissionModel::TS011) {
        recordScalar("WOR standby current mA", currentConsumptionWOR);
        recordScalar("WOR state duration s", worStateSeconds);
        recordScalar("CAD single scan duration s", cadDurationSeconds);
        recordScalar("CAD standby duty fraction", cadDutyFraction);
        recordScalar("TS011 CAD busy skips", cadBusySkips);
        recordScalar("TS011 WOR acquisitions default channel", cadAcquisitionsDefault);
        recordScalar("TS011 WOR acquisitions second channel", cadAcquisitionsSecond);
    }
    if (deviceType == DevType::ENDNODE)
        recordScalar("Final spreading factor", lastSpreadingFactor);
}

} // namespace opp_lora_relay_official
