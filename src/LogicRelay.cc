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

#include "LogicRelay.h"
#include "common.h"
#include "LoRaPacket.h"

namespace opp_lora_relay_official {

Define_Module(LogicRelay);

void LogicRelay::initialize() {

    //baseNode = base;
    // TODO - Generated method body

    RXR_delay = par("listeningWindowRX1").doubleValue() +
                par("listeningWindowRX2").doubleValue() +
                2*par("listeningWindowRXC").doubleValue() +
                par("listeningWindowRXCR").doubleValue() +
                0.5 * par("listeningWindowRXR").doubleValue();

    //simsignal_t relayWakeupSignal = registerSignal("wakeupRelay");

    wakeupRelay = registerSignal("wakeupRelay");
    //subscribe("wakeupRelay", this);
    getSimulation()->getSystemModule()->subscribe("wakeupRelay", this);


    //getSimulation()->getSystemModule()->subscribe("wakeupRelay", this);
}

void LogicRelay::handleMessage(cMessage *msg) {
    LoRaPacket *msgLoRa = dynamic_cast<LoRaPacket*>(msg);

    if (msgLoRa == nullptr)
    {
        EV << simTime() << "L_RELAY \033[1;31m Got unknown message from \033[0m" << msg->getSenderGate() << " \n";
        return;
    }
    //for now only delayed retransmission
    if (msg->isSelfMessage()) {
        send(msgLoRa,"logicOut");
        return;
    }

    if (msg->getSenderModule() == this) {
        std::cout << this->getName() << " discarding self message";
        return;
    }



    switch (msg->getKind())
    {
        //case MsgType::WORRELAYJOIN:
        //case MsgType::WORRELAYJOINACCEPT:
        case MsgType::UPLINK:
        case MsgType::FORWARDUPLINK:
        case MsgType::JOINREQUEST:
            if (msgLoRa->relayRequired){
                if (hasGUI()) {
                    char text[32];
                    sprintf(text, "Retransmission %d", msgLoRa->getSenderModuleId());
                    this->getParentModule()->bubble(text);
                }
                msgLoRa->isRelayed = 1;
                EV << "L_RELAY retransmitting EN to GW";
                send(msgLoRa,"logicOut");                       //TODO: Add delay before transmission
                lastPacketReceive = simTime();     //TODO: ?? shell msg duration be added?
            }
            break;

        case MsgType::FORWARDDOWNLINK:
            if (hasGUI()) {
                char text[32];
                sprintf(text, "Retransmission %d", msgLoRa->getSenderModuleId());
                this->getParentModule()->bubble(text);
            }
            msgLoRa->isRelayed = 1;
            EV << "L_RELAY retransmitting EN to GW";
            send(msgLoRa,"logicOut");                       //TODO: Add delay before transmission
            break;

        case MsgType::MSGWOR:
            //no nothing WOR whould not be retransmitted
            break;

        case MsgType::JOINACCEPT:
        case MsgType::DOWNLINK:
            //if (msgLoRa->relayRequired){
                //if (hasGUI()) {
                    char text[32];
                    sprintf(text, "Retransmission %d", msgLoRa->getSenderModuleId());
                    this->getParentModule()->bubble(text);
                //}
                msgLoRa->isRelayed = 1;
                EV << "L_RELAY retransmitting GW to ED";
                if (lastPacketReceive + RXR_delay > simTime())
                    scheduleAt(lastPacketReceive + RXR_delay,msg);
            //}
            break;
        case MsgType::ACK:
            //direct gateway communication
            if (lastPacketReceive + RXR_delay > simTime())
                scheduleAt(lastPacketReceive + RXR_delay,msgLoRa);
            break;
    }

}

/*
void LogicRelay::receiveSignal(omnetpp::cComponent *src, int id, omnetpp::cObject *value, omnetpp::cObject *details)
{
    EV << "Receiver: Caught signal with value: " << value << "\n";
}

void LogicRelay::receiveSignal(cComponent *src, simsignal_t id, cObject *value, cObject *details) {
    EV << "Receiver: Caught signal with value: " << value << "\n";
}

void LogicRelay::receiveSignal(cComponent *src, simsignal_t id, intval_t value, cObject *details) {
    EV << "Receiver: Caught signal with value: " << value << "\n";
}*/

void LogicRelay::receiveSignal(cComponent *src, simsignal_t id, double value, cObject *details)
{
    if (id == wakeupRelay)
    {

    }
}

} //namespace
