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

#include "LogicEndnode.h"

namespace opp_lora_relay_official {

Define_Module(LogicEndnode);

LogicEndnode::LogicEndnode()
{
//    timerMessage = NULL;
}

LogicEndnode::~LogicEndnode()
{
    cancelAndDelete (timerMessageWindow1);
    cancelAndDelete (timerMessageWindow2);

}

void LogicEndnode::initialize()
{
    updateDaytime();

    transmittingWindowStart1 = par("transmittingWindowStart1").intValue();
    transmittingWindowStart2 = par("transmittingWindowStart2").intValue();
    transmittingWindowDuration1 = par("transmittingWindowDuration1").intValue();
    transmittingWindowDuration2 = par("transmittingWindowDuration2").intValue();

    timerMessageWindow1 = new cMessage("timerMessageWindow1");
    timerMessageWindow1->setKind(SelfMsgType::TXWINDOW1);
    scheduleAt((transmittingWindowStart1 + intrand(transmittingWindowDuration1)) * 60, timerMessageWindow1);

    timerMessageWindow2 = new cMessage("timerMessageWindow2");
    timerMessageWindow2->setKind(SelfMsgType::TXWINDOW2);
    scheduleAt((transmittingWindowStart2 + intrand(transmittingWindowDuration2)) * 60, timerMessageWindow2);

    endDeviceState = EndDeviceState::INIT;
}

void LogicEndnode::handleMessage(cMessage *msg)
{
    updateDaytime();
    if (msg->isSelfMessage()) {

        if (msg->getKind() == SelfMsgType::TXWINDOW1) {
            scheduleAt(
                    simTime() - dayTime * 60 + (transmittingWindowStart1 + intrand(transmittingWindowDuration1)) * 60 + 24*3600,
                    msg);
        }
        if (msg->getKind() == SelfMsgType::TXWINDOW2) {
            scheduleAt(
                    simTime() - dayTime * 60 + (transmittingWindowStart2 + intrand(transmittingWindowDuration2)) * 60 + 24*3600,
                    msg);
        }
        cModule *parent = getParentModule();
        cMessage *job = new cMessage(parent->getFullName());
        job->addPar("msgSize") = 54; //TODO
        send(job, "logicOut");

    }
    else {
        if (msg->arrivedOn("logicIn")) {
            delete msg;
        }
    }
}

void LogicEndnode::updateDaytime()
{
    int64_t timeInMinutesFromTheBeginning = simTime().inUnit(SIMTIME_S) / 60;
    dayTime = timeInMinutesFromTheBeginning % (60 * 24);
}

} //namespace
