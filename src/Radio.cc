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
    tmp_name = parent->getFullName();
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
    cMessage *radioInit = new cMessage("radioInit");
    radioInit->setKind(SelfMsgType::RADIOINIT);
    scheduleAt(simTime(), radioInit);
    radioStatus = RadioActivity::UNKNOWN;
    listeningWindowStart1 = par("listeningWindowStart1").intValue();
    listeningWindowStart2 = par("listeningWindowStart2").intValue();
    listeningWindowDuration1 = par("listeningWindowDuration1").intValue();
    listeningWindowDuration2 = par("listeningWindowDuration2").intValue();
    busyUntil = -1;
    if (deviceType != DevType::GATEWAY) {
        cMessage *timer_listeningWindowStart1 = new cMessage(
                "listeningWindowStart1");
        timer_listeningWindowStart1->setKind(SelfMsgType::WINDOWTIMER);
        scheduleAt(simTime() + listeningWindowStart1 * 60,
                timer_listeningWindowStart1);

        cMessage *timer_listeningWindowStop1 = new cMessage(
                "listeningWindowStop1");
        timer_listeningWindowStop1->setKind(SelfMsgType::WINDOWTIMER);
        scheduleAt(
                simTime() + listeningWindowStart1 * 60
                        + listeningWindowDuration1 * 60,
                timer_listeningWindowStop1);

        cMessage *timer_listeningWindowStart2 = new cMessage(
                "listeningWindowStart2");
        timer_listeningWindowStart2->setKind(SelfMsgType::WINDOWTIMER);
        scheduleAt(simTime() + listeningWindowStart2 * 60,
                timer_listeningWindowStart2);

        cMessage *timer_listeningWindowStop2 = new cMessage(
                "listeningWindowStop2");
        timer_listeningWindowStop2->setKind(SelfMsgType::WINDOWTIMER);
        scheduleAt(
                simTime() + listeningWindowStart2 * 60
                        + listeningWindowDuration2 * 60,
                timer_listeningWindowStop2);
    }
    WATCH(busyUntil);
    WATCH(deviceType);
    WATCH(radioStatus);
    radioStatusVector.setName("radioStatus");
}

void Radio::handleMessage(cMessage *msg) {
    EV << simTime() << " - " << tmp_name << ": " << "handle message "
              << msg->getName() << "\n";
    updateDaytime(); //no.1 - order is important
//    updateRadio(msg); //no.2 - order is important
    cGate *arrivalGate = msg->getArrivalGate();
    if (msg->isSelfMessage()) {
        updateRadio(msg, RadioActivity::TIMER);
    } else if (strcmp(arrivalGate->getName(), "fromLogic") == 0) {
      sendToNetwork(msg);
    } else if (strcmp(arrivalGate->getName(), "radioIn") == 0) {
        receiveFromNetwork(msg);
    }
    EV << simTime() << " - " << tmp_name << ": " << " radio status "
              << radioStatus << "\n";
    radioStatusVector.record(radioStatus);

}

void Radio::updateRadio(cMessage *msg, RadioActivity::ra rae) {
    if (msg->getKind() == SelfMsgType::WINDOWTIMER) {
        EV << simTime() << " - " << tmp_name << ": " << " WINDOWTIMER "
                  << simTime() + 24 * 3600 << "\n";
        scheduleAt(simTime() + 24 * 3600, msg);
        assert(rae == RadioActivity::TIMER);
    }

    if ((rae == RadioActivity::RECEIVING)
            or (rae == RadioActivity::TRANSMITTING)) {
        int msgSize = msg->par("msgSize");
        int spreadingfactor = msg->par("spreadingfactor");
        radioStatus = rae;
        busyUntil = simTime() + transmissionTimeInSec(msgSize, spreadingfactor);
        cMessage *radioUpdate = new cMessage("radioUpdate");
        radioUpdate->setKind(SelfMsgType::TXRXTIMER);
        scheduleAt(busyUntil, radioUpdate);
        EV << simTime() << " - " << tmp_name << ": " << "TX/RX radio busy start"
                  << "\n";
        return;
    }

    if (((radioStatus == RadioActivity::RECEIVING)
            or (radioStatus == RadioActivity::TRANSMITTING))
            and (simTime() < busyUntil)) {
        EV << simTime() << " - " << tmp_name << ": " << "radio is busy" << "\n";
        delete msg;
        return;
    }

    switch (deviceType) {
    case DevType::GATEWAY:
        radioStatus = RadioActivity::LISTENING;
        EV << simTime() << " - " << tmp_name << ": " << "listening start"
                  << "\n";
        break;
    case DevType::RELAY:
    case DevType::ENDNODE:
        if (((dayTime >= listeningWindowStart1)
                and (dayTime < listeningWindowStart1 + listeningWindowDuration1))
                or ((dayTime >= listeningWindowStart2)
                        and (dayTime
                                < listeningWindowStart2
                                        + listeningWindowDuration2))) {
            radioStatus = RadioActivity::LISTENING;
            EV << simTime() << " - " << tmp_name << ": " << "listening start"
                      << "\n";
        } else {
            radioStatus = RadioActivity::RADIOOFF;
            EV << simTime() << " - " << tmp_name << ": " << "radio off" << "\n";
        }
        break;
    }

    if ((msg->getKind() == SelfMsgType::RADIOINIT) or (msg->getKind() == SelfMsgType::TXRXTIMER))
        delete msg;

}
void Radio::receiveFromNetwork(cMessage *msg)
{
    if (isReceivedCorrectly(msg)) {
                updateRadio(msg, RadioActivity::RECEIVING);
                send(msg, "toLogic");
            }
}
void Radio::sendToNetwork(cMessage *msg)
{
    msg->addPar("xpos") = position.x;
          msg->addPar("ypos") = position.y;
          msg->addPar("txPower") = txPower;
          msg->addPar("channel") = 1; //TODO
          msg->addPar("spreadingfactor") = 12; //TODO
          if (keeper->returnRadioDevices().size() > 0)
              updateRadio(msg, RadioActivity::TRANSMITTING);
          for (Node &device : keeper->returnRadioDevices()) // access by reference to avoid copying
          {
              cModule *target =
                      getParentModule()->getParentModule()->getSubmodule(
                              device.name.c_str(), device.index);
              sendDirect(msg->dup(), target, "radioIn");
          }
          delete msg;
}
bool Radio::isReceivedCorrectly(cMessage *msg) { //TODO: add gateway exception - multiple receives
    if (radioStatus == RadioActivity::LISTENING) {
        float rssi = getRssi(msg);
        EV << "I'm " << deviceType << ", received packet with " << rssi << "\n";
        if (rssi > sensitivity) {
            return true;
        }
    }
    EV << simTime() << " - " << tmp_name << ": "
              << "msg not received. rejecting" << "\n";
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

void Radio::updateDaytime() {
    int64_t timeInMinutesFromTheBeginning = simTime().inUnit(SIMTIME_S) / 60;
    dayTime = timeInMinutesFromTheBeginning % (60 * 24);
}

//void Radio::setRadioBusyDuration(cMessage *msg, RadioActivity::ra rae) {
//    int msgSize = msg->par("msgSize");
//    int spreadingfactor = msg->par("spreadingfactor");
//    radioStatus = rae;
//    busyUntil = simTime() + transmissionTimeInSec(msgSize, spreadingfactor);
//}

double Radio::transmissionTimeInSec(int msgSize, int sf) // sf - spreading factor 7 - 12; parameter - rozmiar wiadomosci w bajtach
        { //przeliczanie jak dlugo dana wiadomosc bedzie sie transmitowala - funkcja z symulacji Rafala
    double de = 0;
    if (sf == 11 || sf == 12)
        de = 1;
    int bandwidth = 125;
    int loraHeaderSize = 13;
    int preambleSymbols = 8;
    double explicitHeader = 1;
    int codingRate = 5;
    double I18 = pow(2, sf) / (bandwidth * 1000) * 1000;
    double I19 = (preambleSymbols + 4.25) * I18;
    double roundUp = ceil(
            ((8 * (msgSize + loraHeaderSize) - 4 * sf + 28 + 16
                    - 20 * (1 - explicitHeader)) / (4 * (sf - 2 * de))))
            * codingRate;
    double maximum = fmax(roundUp, 0);
    double I20 = 8 + maximum;
    double I21 = I20 * I18;
    double formula = I19 + I21;

    return formula / 1000.0;
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
