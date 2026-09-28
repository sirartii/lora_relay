#include "LoRaPacket.h"

#include <algorithm>
#include <cmath>

namespace opp_lora_relay_official {

LoRaPacket::LoRaPacket() : LoRaPacket(nullptr, 0) {}

LoRaPacket::LoRaPacket(LoRaPacket *other) : cMessage(*other)
{
    isUplink = other->isUplink;
    isRelayed = other->isRelayed;
    ackRequired = other->ackRequired;
    relayRequired = other->relayRequired;
    encapsulated = other->encapsulated;
    worValid = other->worValid;
    rxrEligible = other->rxrEligible;
    id = other->id;
    length = other->length;
    channel = other->channel;
    worChannel = other->worChannel;
    uplinkChannel = other->uplinkChannel;
    originalKind = other->originalKind;
    originalLength = other->originalLength;
    originalSpreadingFactor = other->originalSpreadingFactor;
    fPort = other->fPort;
    worType = other->worType;
    worFrameCounter = other->worFrameCounter;
    joinEui = other->joinEui;
    devEui = other->devEui;
    preambleSymbols = other->preambleSymbols;
    cadPeriodicityMs = other->cadPeriodicityMs;
    cadToRxSymbols = other->cadToRxSymbols;
    relayClockAccuracyPpm = other->relayClockAccuracyPpm;
    tOffsetMs = other->tOffsetMs;
    forwardStatus = other->forwardStatus;
    relayDataRate = other->relayDataRate;
    forwardingRelayId = other->forwardingRelayId;
    transmissionPower = other->transmissionPower;
    spreadingFactor = other->spreadingFactor;
    rssi = other->rssi;
    rssi_margin = other->rssi_margin;
    snr = other->snr;
    uplinkRssiMetadata = other->uplinkRssiMetadata;
    uplinkSnrMetadata = other->uplinkSnrMetadata;
    uplinkSenderPositionX = other->uplinkSenderPositionX;
    uplinkSenderPositionY = other->uplinkSenderPositionY;
    senderPositionX = other->senderPositionX;
    senderPositionY = other->senderPositionY;
    gateLNS = other->gateLNS;
    senderId = other->senderId;
}

LoRaPacket::LoRaPacket(const char *name, short kind) : cMessage(name, kind)
{
    isUplink = false;
    isRelayed = false;
    ackRequired = false;
    relayRequired = false;
    encapsulated = false;
    worValid = true;
    rxrEligible = false;
    id = 0;
    length = 51;
    channel = 1;
    worChannel = 1;
    uplinkChannel = 1;
    originalKind = MSGUNKNOWN;
    originalLength = 0;
    originalSpreadingFactor = 12;
    fPort = 0;
    worType = WorType::CLASS_A_UPLINK;
    worFrameCounter = 0;
    joinEui = 0x0102030405060708ULL;
    devEui = 0;
    preambleSymbols = 0;
    cadPeriodicityMs = 1000;
    cadToRxSymbols = 8;
    relayClockAccuracyPpm = 40;
    tOffsetMs = 0;
    forwardStatus = 0;
    relayDataRate = 0;
    forwardingRelayId = -1;
    transmissionPower = 14;
    spreadingFactor = 12;
    rssi = 0;
    rssi_margin = 1;
    snr = 0;
    uplinkRssiMetadata = 0;
    uplinkSnrMetadata = 0;
    uplinkSenderPositionX = 0;
    uplinkSenderPositionY = 0;
    senderPositionX = 0;
    senderPositionY = 0;
    gateLNS = 0;
    senderId = 0;
}

int LoRaPacket::calcBitrate()
{
    switch (spreadingFactor) {
        case 12: return 250;
        case 11: return 440;
        case 10: return 980;
        case 9: return 1760;
        case 8: return 3125;
        case 7: return 5470;
        default: return 0;
    }
}

float LoRaPacket::calcSensitivity()
{
    switch (spreadingFactor) {
        case 12: return 20;
        case 11: return 17.5;
        case 10: return 15;
        case 9: return 12.5;
        case 8: return 10;
        case 7: return 7.5;
        default: return 0;
    }
}

double LoRaPacket::symbolTime() const
{
    return std::pow(2.0, spreadingFactor) / 125000.0;
}

int LoRaPacket::calcMsgPreamble() const
{
    if (isWOR())
        return preambleSymbols > 0 ? preambleSymbols : static_cast<int>(std::ceil(1.0 / symbolTime()));
    return 8;
}

bool LoRaPacket::isWOR() const { return getKind() == MsgType::MSGWOR; }
bool LoRaPacket::isWorAck() const { return getKind() == MsgType::MSGWORACK; }

int LoRaPacket::autodetectUplink()
{
    switch (getKind()) {
        case MSGUNKNOWN:
        case MSGWOR:
        case JOINREQUEST:
        case FORWARDUPLINK:
        case UPLINK:
        case TIMEOUT:
            isUplink = true;
            break;
        case MSGWORACK:
        case JOINACCEPT:
        case DOWNLINK:
        case FORWARDDOWNLINK:
        case ACK:
            isUplink = false;
            break;
        default:
            break;
    }
    return isUplink;
}

int LoRaPacket::phyPayloadLength() const
{
    // Control-frame lengths already describe their complete on-air payload.
    if (isWOR() || isWorAck() || getKind() == MsgType::JOINREQUEST ||
        getKind() == MsgType::JOINACCEPT || getKind() == MsgType::ACK)
        return length;
    return length + LORAWAN_FRAME_OVERHEAD;
}

float LoRaPacket::calcMsgDuration() const
{
    const int sf = spreadingFactor;
    const double lowDataRateOptimization = (sf == 11 || sf == 12) ? 1.0 : 0.0;
    const int bandwidthKhz = 125;
    const int preamble = calcMsgPreamble();
    const double explicitHeader = 1;
    const int codingRate = 5;
    const double symbolMs = std::pow(2.0, sf) / (bandwidthKhz * 1000.0) * 1000.0;
    const double preambleMs = (preamble + 4.25) * symbolMs;
    const double encoded = std::ceil(
        (8 * phyPayloadLength() - 4 * sf + 28 + 16 - 20 * (1 - explicitHeader)) /
        (4 * (sf - 2 * lowDataRateOptimization))) * codingRate;
    const double payloadSymbols = 8 + std::max(encoded, 0.0);
    return static_cast<float>((preambleMs + payloadSymbols * symbolMs) / 1000.0);
}

double LoRaPacket::CalculateCADLength(int sf)
{
    switch (sf) {
        case 12: return 32.50;
        case 11: return 18.68;
        case 10: return 8.66;
        case 9: return 5.05;
        case 8: return 3.06;
        case 7: return 1.96;
        default: return 0;
    }
}

std::string LoRaPacket::GetPacketName() const
{
    switch (getKind()) {
        case MSGUNKNOWN: return "MSGUNKNOWN";
        case MSGWOR: return "WOR";
        case MSGWORACK: return "WOR ACK";
        case JOINREQUEST: return "JOINREQUEST";
        case JOINACCEPT: return "JOINACCEPT";
        case FORWARDUPLINK: return "FORWARDUPLINK";
        case FORWARDDOWNLINK: return "FORWARDDOWNLINK";
        case UPLINK: return "UPLINK";
        case DOWNLINK: return "DOWNLINK";
        case ACK: return "ACK";
        case TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}

LoRaPacket::~LoRaPacket() = default;
LoRaPacket *LoRaPacket::dupLoRa() { return new LoRaPacket(this); }

} // namespace opp_lora_relay_official
