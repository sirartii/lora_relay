#include "Radio.h"

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

    currentConsumptionRx = par("currentConsumptionRx").doubleValue();
    currentConsumptionTx = par("currentConsumptionTx").doubleValue();
    currentConsumptionIdle = par("currentConsumptionIdle").doubleValue();
    currentConsumptionWOR = par("currentConsumptionWOR").doubleValue();
    currentConsumptionBaseOffset = par("currentConsumptionBaseOffset").doubleValue();
    lteAttachEnergyMAs = par("lteAttachEnergyMAs").doubleValue();
    lteTransmitEnergyMAs = par("lteTransmitEnergyMAs").doubleValue();

    radioRXTimer = new cMessage("classAReceiveTimer");
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
                        scheduleAt(simTime() + ts011WorAckDelay, open);
                    }
                }
                else if (deviceType != DevType::GATEWAY && txKind != MsgType::MSGWORACK &&
                         !disableClassAWindows) {
                    activeReceiveChannel = txChannel;
                    scheduleClassAReception(includeRxr);
                }
                disableClassAWindows = false;
                delete msg;
                return;
            }
            case SelfMsgType::WORACK_RX_OPEN: {
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
                updateRadio();
                msg->setKind(SelfMsgType::RXC);
                msg->par("windowIndex") = 2;
                scheduleAt(simTime() + listeningWindowRXC, msg);
                return;
            case SelfMsgType::RX2:
                updateRadio();
                if (rxrPending && !downlinkReceivedInClassA) {
                    msg->setKind(SelfMsgType::RXC);
                    msg->par("windowIndex") = 3;
                    scheduleAt(simTime() + listeningWindowRXCR, msg);
                }
                else {
                    msg->par("windowIndex") = 0;
                    if (!sendQueue.empty())
                        sendToNetwork(nullptr);
                }
                return;
            case SelfMsgType::RXR:
                updateRadio();
                rxrPending = false;
                if (!sendQueue.empty())
                    sendToNetwork(nullptr);
                if (msg != radioRXTimer)
                    delete msg;
                return;
            case SelfMsgType::RELAY_RX_TIMEOUT:
                updateRadio();
                delete msg;
                return;
            case SelfMsgType::RXTIMER:
                if (currentRcvMessage != nullptr) {
                    if (deviceType == DevType::IPBRIDGE && currentRcvMessage->isUplink &&
                        currentRcvMessage->relayRequired) {
                        energyConsumptionLteAttach += lteAttachEnergyMAs;
                        energyConsumptionLteTransmit += lteTransmitEnergyMAs;
                        ++lteAttachCount;
                        ++lteTransmitCount;
                    }
                    if (!currentRcvMessage->isUplink && !currentRcvMessage->isWorAck())
                        downlinkReceivedInClassA = true;
                    send(currentRcvMessage, "toLogic");
                    currentRcvMessage = nullptr;
                }
                radioUpdateReceive = nullptr;
                rssiOfCurrentReceive = sensitivity - 1;
                if (!sendQueue.empty())
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
        if (msg->getKind() == MsgType::RESET_ADR) {
            if (lastSpreadingFactor < 12)
                ++lastSpreadingFactor;
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
    updateRadioActivity(RadioActivity::LISTENING);
    timer->setKind(index == 1 ? SelfMsgType::RX1 : index == 2 ? SelfMsgType::RX2 : SelfMsgType::RXR);
    if (index == 2)
        activeReceiveChannel = ts011Rx2Channel;
    else if (index == 3)
        activeReceiveChannel = ts011RxrChannel;
    const double duration = index == 1 ? listeningWindowRX1 : index == 2 ? listeningWindowRX2 : listeningWindowRXR;
    scheduleAt(simTime() + duration, timer);
}

void Radio::scheduleClassAReception(bool includeRxr)
{
    if (radioRXTimer->isScheduled())
        cancelEvent(radioRXTimer);
    rxrPending = includeRxr;
    downlinkReceivedInClassA = false;
    radioRXTimer->setKind(SelfMsgType::RXC);
    radioRXTimer->par("windowIndex") = 1;
    scheduleAt(simTime() + listeningWindowRXC, radioRXTimer);
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
    if (radioActivity == RadioActivity::TRANSMITTING || radioActivity == RadioActivity::RECEIVING)
        return true; // deliver so collision/capture logic can evaluate it
    return roleAcceptsPacket(packet);
}

bool Radio::relayScanOverlaps(const LoRaPacket *packet, simtime_t *scanTime) const
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

    const double start = simTime().dbl();
    const double symbol = packet->symbolTime();
    const double earliestScan = start - ts011ChannelScanSymbols * symbol;
    const double latestScan = start + packet->calcMsgPreamble() * symbol -
                              (6 + ts011CadToRxSymbols) * symbol;
    const double n = std::ceil((earliestScan - offset) / ts011CadPeriodicity - 1e-12);
    const double candidate = offset + std::max(0.0, n) * ts011CadPeriodicity;
    if (candidate + 1e-12 < earliestScan || candidate - 1e-12 > latestScan)
        return false;
    if (scanTime != nullptr)
        *scanTime = SimTime(candidate);
    return true;
}

void Radio::receiveFromNetwork(LoRaPacket *msg)
{
    if (msg->senderId != myKeeperIndex && deviceType == DevType::ENDNODE) {
        delete msg;
        return;
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
    const double relayLinkMarginDb = par("relayLinkMarginDb").doubleValue();
    if (relayAccessLink)
        rssi += relayLinkMarginDb;
    if (ipBridgeAccessUplink)
        rssi += par("ipBridgeLinkMarginDb").doubleValue();
    if (reliableRelayBackhaul)
        rssi = std::max(rssi, realSensitivity + relayLinkMarginDb);
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
        receiveOk = rssi > realSensitivity && msg->worValid && relayScanOverlaps(msg);
    }

    keeper->recordFrameReception(myKeeperIndex, msg->getSenderModuleId(), rssi, receiveOk);
    if (!receiveOk) {
        delete msg;
        return;
    }

    if (deviceType == DevType::RELAY && msg->isWOR()) {
        simtime_t scan;
        relayScanOverlaps(msg, &scan);
        const double preambleEnd = simTime().dbl() + msg->calcMsgPreamble() * msg->symbolTime();
        msg->tOffsetMs = static_cast<int>(std::ceil((preambleEnd - scan.dbl()) * 1000.0));
        relayExpectedDataChannel = msg->uplinkChannel;
        LoRaPacket ack("WOR ACK timing", MsgType::MSGWORACK);
        ack.length = TS011_WOR_ACK_LENGTH;
        ack.spreadingFactor = msg->spreadingFactor;
        relayExpectedUplinkUntil = simTime() + msg->calcMsgDuration() + ts011WorAckDelay +
            ack.calcMsgDuration() + ts011WorDataDelay + ts011UplinkListenTimeout;
        scheduleAt(relayExpectedUplinkUntil,
                   new cMessage("ts011UplinkListenTimeout", SelfMsgType::RELAY_RX_TIMEOUT));
    }

    const double duration = msg->calcMsgDuration();
    updateRadioActivity(RadioActivity::RECEIVING);
    busyUntil = simTime() + duration;
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
        if (sendQueue.empty())
            return;
        auto *next = new LoRaPacket(sendQueue.front());
        sendQueue.pop_front();
        directSendWithDelete(next);
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
    const double duration = msg->calcMsgDuration();
    busyUntil = simTime() + duration;
    auto *done = new cMessage("radioTransmitComplete", SelfMsgType::TXTIMER);
    done->addPar("txKind") = msg->getKind();
    done->addPar("txChannel") = msg->channel;
    done->addPar("worAckExpected") = msg->isWOR() && msg->worType == WorType::CLASS_A_UPLINK;
    done->addPar("includeRxr") = msg->rxrEligible;
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
            break;
        case RadioActivity::TRANSMITTING:
            energyConsumptionTx += duration * currentConsumptionTx;
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
    updateRadioActivity(dailyWindow ? RadioActivity::LISTENING : RadioActivity::RADIOOFF);
}

bool EndDeviceRadio::roleAcceptsPacket(const LoRaPacket *packet) const
{
    return !packet->encapsulated && !packet->isUplink && packet->channel == activeReceiveChannel &&
           (radioActivity == RadioActivity::LISTENING || radioActivity == RadioActivity::RECEIVING);
}

void RelayRadio::updateIdleRadioState()
{
    if (retransmissionModel == RetransmissionModel::TS011) {
        if (simTime() < relayExpectedUplinkUntil) {
            activeReceiveChannel = relayExpectedDataChannel;
            updateRadioActivity(RadioActivity::LISTENING);
        }
        else {
            updateRadioActivity(RadioActivity::WOR);
        }
    }
    else if (simTime() < relayListeningUntil) {
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
        return packet->worValid && relayScanOverlaps(packet);
    if (retransmissionModel == RetransmissionModel::TS011 && packet->isUplink && packet->relayRequired)
        return simTime() < relayExpectedUplinkUntil && packet->channel == relayExpectedDataChannel;
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
    recordScalar("LTE attach count", lteAttachCount);
    recordScalar("LTE transmit count", lteTransmitCount);
    recordScalar("Radio collision count", collisionCount);
}

} // namespace opp_lora_relay_official
