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

#include "LogicGateway.h"
#include "LoRaPacket.h"

using namespace omnetpp;

namespace opp_lora_relay_official {

Define_Module(LogicGateway);

void LogicGateway::initialize() {

    //getAncestorPar("xpos").setDoubleValue(posX);
    //getAncestorPar("ypos").setDoubleValue(posY);

}

void LogicGateway::handleMessage(cMessage *msg) {
    if (msg->isSelfMessage()) {

    } else {

        LoRaPacket *LoRaMsg = dynamic_cast<LoRaPacket *>(msg);
        if (msg->arrivedOn("logicIn")) {
            if (LoRaMsg != nullptr )
            {
                EV << simTime() << " LNS Gateway got correct loRa Wan packet!";
            }
            if (LoRaMsg->autodetectUplink())
            {
                //dodać
                send(((LoRaPacket*)msg)->dupLoRa(),"json$o");               //Not fully understand why need to be duplicate but otherwise getting runtime error
            }
        }
        if (msg->arrivedOn("json$i"))
        {
            send(((LoRaPacket*)msg)->dupLoRa(),"logicOut");             //Not fully understand why need to be duplicate but otherwise getting runtime error
        }
    }
}

} //namespace
