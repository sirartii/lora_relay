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

#ifndef LORAPACKET_H_
#define LORAPACKET_H_

#include <omnetpp/cpacket.h>
#include <cstdint>
#include "common.h"

namespace opp_lora_relay_official {

class LoRaPacket : public omnetpp::cMessage
{
public:
    LoRaPacket();
    LoRaPacket(LoRaPacket* duplicate);
    LoRaPacket(const char *name=nullptr, short kind=0);
    LoRaPacket* dupLoRa();

    double CalculateCADLength(int SpreadingFactor);
    int calcBitrate();
    float calcSensitivity();
    int calcMsgPreamble() const;
    bool isWOR() const;
    bool isWorAck() const;
    int autodetectUplink();
    float calcMsgDuration() const;
    double symbolTime() const;
    std::string GetPacketName() const;


    virtual ~LoRaPacket();

    bool isUplink;
    bool isRelayed;
    bool ackRequired;
    bool relayRequired;
    bool encapsulated;
    bool worValid;
    bool rxrEligible;


    unsigned char id;
    unsigned char length;
    unsigned char channel;
    unsigned char worChannel;
    unsigned char uplinkChannel;
    unsigned char originalKind;
    unsigned char originalLength;
    unsigned char originalSpreadingFactor;
    unsigned char fPort;

    WorType worType;
    unsigned int worFrameCounter;
    std::uint64_t joinEui;
    std::uint64_t devEui;
    int preambleSymbols;
    int cadPeriodicityMs;
    int cadToRxSymbols;
    int relayClockAccuracyPpm;
    int tOffsetMs;
    int forwardStatus;
    int relayDataRate;
    int forwardingRelayId;

    float transmissionPower;
    short spreadingFactor;
    float rssi;
    float rssi_margin;
    float snr;
    float uplinkRssiMetadata;
    float uplinkSnrMetadata;
    float uplinkSenderPositionX;
    float uplinkSenderPositionY;

    float senderPositionX;
    float senderPositionY;

    int gateLNS;
    int senderId;
};

} /* namespace opp_lora_relay_official */

#endif /* LORAPACKET_H_ */
