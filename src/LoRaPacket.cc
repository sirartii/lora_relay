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

#include "LoRaPacket.h"
#include "common.h"

namespace opp_lora_relay_official {

LoRaPacket::LoRaPacket() : cMessage()
{
    isUplink = false;
    isRelayed = false;
    relayRequired = false;
    ackRequired = false;

    //WATCH(isUplink);
    //WATCH(isRelayed);
    //WATCH(relayRequired);
    //WATCH(ackRequired);

    // TODO Auto-generated constructor stub
}

LoRaPacket::LoRaPacket(LoRaPacket* duplicate) : cMessage(*duplicate)
{
    isUplink = duplicate-> isUplink;
    ackRequired = duplicate->ackRequired;
    relayRequired = duplicate->relayRequired;
    isRelayed = duplicate->isRelayed;

    id = duplicate->id;
    length = duplicate->length;
    channel = duplicate->channel;

    transmissionPower = duplicate->transmissionPower;
    spreadingFactor = duplicate->spreadingFactor;
    rssi = duplicate->rssi;

    senderPositionX = duplicate->senderPositionX;
    senderPositionY = duplicate->senderPositionY;

    gateLNS = duplicate->gateLNS;
}


LoRaPacket::LoRaPacket(const char *name, short kind) : cMessage(name, kind)
{
    isUplink = false;

    ackRequired = false;
    isRelayed = false;
    relayRequired = false;
    ackRequired = false;

    id = 0;
    length = 51;
    channel = 1;

    transmissionPower = 14;
    spreadingFactor = 12;
    rssi = 0;

    senderPositionX = 0;
    senderPositionY = 0;

    gateLNS = 0;
}

int LoRaPacket::calcBitrate()
{
    switch (this->spreadingFactor)
    {
        case 12:
            return 250;
        case 11:
            return 440;
        case 10:
            return 980;
        case 9:
            return 1760;
        case 8:
            return 3125;
        case 7:
            return 5470;
        default:
            return 0;
    }
}

int LoRaPacket::calcMsgPreamble()
{

    if (isWOR())
        return 1000 / calcBitrate();

    return 8;
}

int LoRaPacket::isWOR()
{
    if (this->getKind() == MsgType::MSGWOR)
        return 1;

    return 0;
}

int LoRaPacket::autodetectUplink()
{
    switch (this->getKind())
    {
        case MSGUNKNOWN:
        case MSGWOR:
        case JOINREQUEST:
        case FORWARDUPLINK:
        case UPLINK:
        case TIMEOUT:
            isUplink = 1;
            break;

        case JOINACCEPT:
        case DOWNLINK:
        case FORWARDDOWNLINK:
        case ACK:
            isUplink = 0;
            break;

    }
    return isUplink;
}

float LoRaPacket::calcMsgDuration()
{
    int msgSize = length;
    int sf = spreadingFactor;

    double de = 0;
    if (sf == 11 || sf == 12)
        de = 1;
    int bandwidth = 125;
    int loraHeaderSize = 13;
    int preambleSymbols = calcMsgPreamble();


    double explicitHeader = 1;
    int codingRate = 5;
    double I18 = pow(2, sf) / (bandwidth * 1000) * 1000;
    double I19 = (preambleSymbols + 4.25) * I18;
    double roundUp = ceil(
            ((8 * (msgSize + loraHeaderSize) - 4 * sf + 28 + 16 - 20 * (1 - explicitHeader)) / (4 * (sf - 2 * de))))
            * codingRate;
    double maximum = fmax(roundUp, 0);
    double I20 = 8 + maximum;
    double I21 = I20 * I18;
    double formula = I19 + I21;

    return formula / 1000.0;
}

double LoRaPacket::CalculateCADLength(int SpreadingFactor)
{

    switch (SpreadingFactor){
        case 12:
            return (0.5 + 32.00);
        case 11:
            return (0.5 + 18.18);
        case 10:
            return (0.5 + 8.16);
        case 9:
            return (0.5 + 4.55);
        case 8:
            return (0.5 + 2.56);
        case 7:
            return (0.5 + 1.46);
        default:
            return 0;
    }
}

std::string LoRaPacket::GetPacketName()
{
    switch (this->getKind())
    {
        case MSGUNKNOWN: return "MSGUNKNOWN";
        case MSGWOR: return "WOR";
        case JOINREQUEST: return "JOINREQUEST";
        case JOINACCEPT: return "JOINACCEPT";
        case FORWARDUPLINK: return "FORWARDUPLINK";
        case FORWARDDOWNLINK: return "FORWARDDOWNLINK";
        case UPLINK: return "UPLINK ";
        case DOWNLINK: return "DOWNLINK";
        case ACK: return "ACK";
        case TIMEOUT: return "TIMEOUT";

    }
    return "UNKNONW";
}

LoRaPacket::~LoRaPacket()
{
    // TODO Auto-generated destructor stub
}

LoRaPacket* LoRaPacket::dupLoRa()
{
    return new LoRaPacket(this);
}

} /* namespace opp_lora_relay_official */
