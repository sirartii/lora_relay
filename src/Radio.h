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
#include <math.h>       /* log10 */
#include <list>

using namespace omnetpp;

namespace opp_lora_relay_official {

struct DevType {
    enum dt {
        DEVUNKNOWN, ENDNODE, RELAY, GATEWAY
    };
};

struct RadioActivity {
    enum ra {
        UNKNOWN, RADIOOFF, LISTENING, RECEIVING, TRANSMITTING, TIMER
    };
};

char *RadioActivityTypes[] =
{
    "UNKNOWN",
    "RADIOOFF",
    "LISTENING",
    "RECEIVING",
    "TRANSMITTING",
    "TIMER"
};

struct SelfMsgType {
    enum ra {
        UNKNOWN, RADIOINIT, WINDOWTIMER, RXTIMER, TXTIMER, RXC, RX1, RX2
    };
};

struct Coords {
    double x, y;
};
/**
 * Generates messages; see NED file for more info.
 */
class Radio: public cSimpleModule {

protected:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    virtual void finish();
    bool isReceivedCorrectly(cMessage *msg);
    double getRssi(cMessage *msg);
    double calculateReceivedPower(float pSend, double distance);
    void updateRadio();
    void setRadioStatus(cMessage *msg);
    void updateDaytime();
    void sendToNetwork(cMessage *msg);
    void receiveFromNetwork(cMessage *msg);
    void directSendWithDelete(cMessage *msg);


//    void setRadioBusyDuration(cMessage *msg, RadioActivity::ra rae);
    double transmissionTimeInSec(int msgSize, int sf);

    DevType::dt deviceType;
    RadioActivity::ra radioStatus;
    Keeper *keeper;
    Coords position;
    double txPower;
    double sensitivity;
    int dayTime;
    simtime_t busyUntil;
    std::string tmp_name;
    std::list<cMessage> sendQueue;


//    cMessage *radioRX1Timer;
//    cMessage *radioRX2Timer;
    cMessage *radioRXTimer;

    int listeningWindowStart1;
    int listeningWindowStart2;
    int listeningWindowDuration1;
    int listeningWindowDuration2;
    double listeningWindowRX1;
    double listeningWindowRX2;
    double listeningWindowRXC;
    cOutVector radioStatusVector;
};

}
;
// namespace

#endif
