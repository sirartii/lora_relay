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

#include "Radio.h"

namespace opp_lora_relay_official {

Define_Module(Radio);

void Radio::initialize() {
    cModule *parent = getParentModule();
    std::string s = parent->getName();
    int index = -1;
    if (parent->isVector())
        index = parent->getIndex();

    std::cout << s << std::endl;

    if (s.rfind("endnode", 0) == 0) {
        deviceType = DevType::ENDNODE;
    } else if (s.rfind("relay", 0) == 0) {
        deviceType = DevType::RELAY;
    } else if (s.rfind("gateway", 0) == 0) {
        deviceType = DevType::GATEWAY;
    } else {
        deviceType = DevType::DEVUNKNOWN;
    }
    keeper = check_and_cast<Keeper*>(
            parent->getParentModule()->getSubmodule("keeper"));

    keeper->registerMe(s, index);

    position.x = getAncestorPar("xpos").doubleValue();
    position.y = getAncestorPar("ypos").doubleValue();
    sensitivity = par("sensitivity").doubleValue();
    txPower = par("txPower").doubleValue();
    cMessage *radioUpdate = new cMessage("radioUpdate");
    scheduleAt(simTime(), radioUpdate);
    radioStatus = RadioActivity::UNKNOWN;
    listeningWindowStart1 = par("listeningWindowStart1").intValue();
    listeningWindowStart2 = par("listeningWindowStart2").intValue();
    listeningWindowDuration1 = par("listeningWindowDuration1").intValue();
    listeningWindowDuration2 = par("listeningWindowDuration2").intValue();
}

void Radio::handleMessage(cMessage *msg) {
    updateDaytime(); //no.1 - order is important
//    updateRadio(msg); //no.2 - order is important

    cGate *arrivalGate = msg->getArrivalGate();
    if (msg->isSelfMessage()) {
        updateRadio(msg);
    } else if (strcmp(arrivalGate->getName(), "fromLogic") == 0) {
        msg->addPar("xpos") = position.x;
        msg->addPar("ypos") = position.y;
        msg->addPar("txPower") = txPower;
        msg->addPar("channel") = 1; //TODO
        msg->addPar("spreadingfactor") = 12; //TODO
        for (Node &device : keeper->returnRadioDevices()) // access by reference to avoid copying
        {
            cModule *target =
                    getParentModule()->getParentModule()->getSubmodule(
                            device.name.c_str(), device.index);
            sendDirect(msg->dup(), target, "radioIn");
        }
        delete msg;
    } else if (strcmp(arrivalGate->getName(), "radioIn") == 0) {
        if (isReceivedCorrectly(msg))
        {
            setRadioBusyDuration(msg);
            send(msg, "toLogic");
        }
    }
}

bool Radio::isReceivedCorrectly(cMessage *msg) {
    if (radioStatus == RadioActivity::LISTENING) {
        float rssi = getRssi(msg);
        EV << "I'm " << deviceType << ", received packet with " << rssi << "\n";
        if (rssi > sensitivity) {
            return true;
        }
    }
    delete msg;
    return false;

}

double Radio::getRssi(cMessage *msg) {
//    std::cout << "Received from: " << msg->par("xpos").doubleValue() << ", "
//            << msg->par("ypos").doubleValue() << "\n";
    double distance = sqrt(
            pow(position.x - msg->par("xpos").doubleValue(), 2)
                    + pow(position.y - msg->par("ypos").doubleValue(), 2));
    return calculateReceivedPower(msg->par("txPower").doubleValue(), distance);
}

double Radio::calculateReceivedPower(float pSend, double distance) //Okumura lite
        {
    if (int(distance) == 0) {
        return pSend;
    }
    float K1 = 127.5;
    float K2 = 35.2;
    float PL = K1 + K2 * log10(distance / 1000.0);

    float pRec = pSend - PL;
    if (pRec > pSend)
        pRec = pSend;
    return pRec;

}

void Radio::updateRadio(cMessage *msg) {
    if (radioStatus == RadioActivity::UNKNOWN) {
        switch (deviceType) {
        case DevType::GATEWAY:
            radioStatus = RadioActivity::LISTENING;
            break;
        case DevType::RELAY:
        case DevType::ENDNODE:
            if (((dayTime >= listeningWindowStart1)
                    and (dayTime
                            < listeningWindowStart1 + listeningWindowDuration1))
                    or ((dayTime >= listeningWindowStart2)
                            and (dayTime
                                    < listeningWindowStart2
                                            + listeningWindowDuration2))) {
                radioStatus = RadioActivity::LISTENING;
                //TODO: selfmsg to set radio off
            } else {
                radioStatus = RadioActivity::RADIOOFF;
                //TODO: selfmsg to set listening
            }
            break;
        }
    }
    delete msg;
}

void Radio::updateDaytime() {
    int64_t timeInMinutesFromTheBeginning = simTime().inUnit(SIMTIME_S) / 60;
    dayTime = timeInMinutesFromTheBeginning % (60 * 24);
}


void Radio::setRadioBusyDuration(cMessage *msg)
{
    int payloadsize = msg->par("payloadsize");
    int channel = msg->par("channel");
    int spreadingfactor = msg->par("spreadingfactor");
    radioStatus = RadioActivity::RECEIVING; //TODO: or Transmission - switch needed

    //TODO: calculate duration of transmission and set selfmsg

}

//
//
//    def returnSpreadingFactor(self, SINR, minSINR):
//        if SINR is None or SINR < minSINR:
//            return None, None
//        sf = 12
//        w = 1.0
//        for i in range(5):
//            if SINR >= minSINR + i * 3 + 3:
//                sf -= 1
//                w /= 2.0
//            else:
//                break
//        return sf, w
//
//
//    def setSpreadingFactor(self):
//        for n in range(len(self.ue)):
//            for m in range(len(self.ue)):
//                if n != m:
//                    value = 7
//                    if self.attenuationTable[n][m] > 20:
//                        value = 8
//                    elif self.attenuationTable[n][m] > 10:
//                        value = 9
//                    self.SFtable[n][m] = value
//                    self.SFtable[m][n] = value

}
;
// namespace
