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

#ifndef __OPP_LORA_RELAY_OFFICIAL_SOURCE_H
#define __OPP_LORA_RELAY_OFFICIAL_SOURCE_H

#include <omnetpp.h>
#include "Keeper.h"
#include "LogicEndnode.h"
#include <math.h>       /* log10 */
#include <list>
#include "common.h"
#include "LoRaPacket.h"
using namespace omnetpp;

namespace opp_lora_relay_official {

//char *RadioActivityTypes[] = { "UNKNOWN", "RADIOOFF", "LISTENING", "RECEIVING", "TRANSMITTING", "TIMER" };
//char *RadioMessageTypes[] = { "JOIN_REQUEST", "RELAY_JOIN_REQUEST", "DATA_FRAME"}

enum RadioFrameType {
    JOIN_REQUEST = 0,
    RELAY_JOIN_REQUEST = 1,
    DATA_FRAME = 2,
};

class Radio : public cSimpleModule, public cListener
{
public:
    bool isListening();
    bool isOffOrUnknown() const noexcept;

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void receiveSignal(cComponent *src, simsignal_t id, double value, cObject *details) override;
    virtual void finish() override;
    using omnetpp::cListener::finish;       //in order not to override finish from cListener

    bool isReceivedCorrectly(cMessage *msg, double rssi);
    double getRssi(LoRaPacket *msg);
    double calculateReceivedPower(float pSend, double distance);
    void updateRadio();
    void setRadioStatus(cMessage *msg);
    void updateDaytime();
    void sendToNetwork(LoRaPacket *msg);
    void receiveFromNetwork(LoRaPacket *msg);
    void directSendWithDelete(LoRaPacket *msg);

//    void setRadioBusyDuration(cMessage *msg, RadioActivity::ra rae);
    void updateRadioActivity(RadioActivity::ra newRA);
    void dropByCollision(LoRaPacket* lp);
    static inline double readCoord(cModule* m, const char* name, int tagIndex);

    double rssiOfCurrentReceive;
    DevType deviceType;
    RadioActivity::ra radioActivity;
    RetransmissionModel retransmissionModel;

    Keeper *keeper;
    int myKeeperIndex;

    Coords position;
    double txPower;
    double sensitivity;
    int dayTime;
    simtime_t busyUntil;
    std::string tmp_name;
    std::list<LoRaPacket> sendQueue;
    bool listeningWindowRXRactive;
    bool disableLAT;
    int collisionCount;
    int lastSpreadingFactor;

//    cMessage *radioRX1Timer;
//    cMessage *radioRX2Timer;
    cMessage *radioRXTimer;
    cMessage *timer_listeningWindowStart1;
    cMessage *timer_listeningWindowStop1;
    cMessage *timer_listeningWindowStart2;
    cMessage *timer_listeningWindowStop2;

    LoRaPacket *currentRcvMessage;

    int listeningWindowStart1;
    int listeningWindowStart2;
    int listeningWindowDuration1;
    int listeningWindowDuration2;

    double listeningWindowRX1;
    double listeningWindowRX2;
    double listeningWindowRXR;
    double listeningWindowRXC;
    double listeningWindowRXCR;

    //cOutVector radioStatusVector;
    //cOutVector energyConsumptionVector;

    double currentConsumptionRx;
    double currentConsumptionTx;
    double currentConsumptionIdle;
    double currentConsumptionWOR;

    double energyConsumptionRx;
    double energyConsumptionTx;
    double energyConsumptionIdle;
    double energyConsumptionWOR;

    simsignal_t wakeupRelay;
    simtime_t relayListeningUntil;
    simtime_t lastRadioActivityChange;
    cMessage *radioUpdateReceive;

};

}
;
// namespace

#endif
