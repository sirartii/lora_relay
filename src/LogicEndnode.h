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

#ifndef __OPP_LORA_RELAY_OFFICIAL_LOGICENDNODE_H_
#define __OPP_LORA_RELAY_OFFICIAL_LOGICENDNODE_H_

#include <omnetpp.h>
#include "common.h"
#include "Keeper.h"

using namespace omnetpp;

namespace opp_lora_relay_official {

class LogicEndnode : public cSimpleModule
{
public:
    LogicEndnode();
    virtual ~LogicEndnode();
    int myKeeperIndex;

private:
    int dayTime; //minutes
    int transmittingWindowStart1;
    int transmittingWindowStart2;
    int transmittingWindowDuration1;
    int transmittingWindowDuration2;
    cMessage *timerMessageWindow1;
    cMessage *timerMessageWindow2;

    int windowsPerDay;                 // 1 lub 2
    int framesPerWindowMin;            // min. ramek w oknie (>=1)
    int framesPerWindowMax;            // max. ramek w oknie (>=min)
    int framesRemainingWin1;           // ile pozostało do wysłania w bieżącym oknie #1
    int framesRemainingWin2;           // analogicznie dla okna #2

    EndDeviceState endDeviceState;
    int MsgAfterWOR;
    int packetCounter;


    //unsigned char packetCounter;
    int lastACK;
    unsigned char waitingResponse;
    int missedResponse;
    Keeper *keeper;

    RetransmissionModel retransmissionModel;
    double RTCdrift;
    simtime_t lastACKTime;
    simtime_t lastOnline;

    double offlineMax;
    double offlineTotal;

    simsignal_t wakeupRelay;

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void handleParameterChange(const char * parname ) override;
    virtual void finish() override;

    void recordOnline();
    void updateDaytime();
    void updateLogicalState(EndDeviceState newState);
};

} //namespace

#endif
