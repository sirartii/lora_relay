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
#include "LoRaPacket.h"

namespace opp_lora_relay_official {

Define_Module(Radio);

void Radio::initialize()
{
    listeningWindowRXRactive = true;
    cModule *parent = getParentModule();
    std::string s = parent->getName();
    tmp_name = parent->getFullName();
    int index = -1;
    if (parent->isVector())
        index = parent->getIndex();

    std::cout << s << std::endl;
    keeper = check_and_cast<Keeper*>(parent->getParentModule()->getSubmodule("keeper"));

    if (s.rfind("endnode", 0) == 0) {
        deviceType = DevType::ENDNODE;
        myKeeperIndex = keeper->registerMe(s, index, deviceType, this);
        //(check_and_cast<LogicEndnode*>(parent->getParentModule()))->myKeeperIndex = myKeeperIndex;
    }
    else if (s.rfind("relay", 0) == 0) {
        deviceType = DevType::RELAY;
        myKeeperIndex =keeper->registerMe(s, index, deviceType, this);
    }
    else if (s.rfind("gateway", 0) == 0) {
        deviceType = DevType::GATEWAY;
        myKeeperIndex = keeper->registerMe(s, index, deviceType, this);
    }
    else if (s.rfind("ipbridge", 0) == 0 || s.rfind("ipb", 0) == 0) {
        deviceType = DevType::IPBRIDGE;               // lub nowy typ jeśli chcesz
        myKeeperIndex = keeper->registerMe(s, index, deviceType, this);
    }
    else {
        deviceType = DevType::DEVUNKNOWN;
    }



    cModule* host = this->getParentModule();
    position.x = host->par("xpos").doubleValue();
    position.y = host->par("ypos").doubleValue();


    sensitivity = par("sensitivityAllRadios").doubleValue();
    txPower = par("txPower").doubleValue();
    cMessage *radioInit = new cMessage("radioInit");
    radioInit->setKind(SelfMsgType::RADIOINIT);
    scheduleAt(simTime(), radioInit);

    radioActivity = RadioActivity::UNKNOWN;
    updateRadioActivity(RadioActivity::RADIOOFF);

    radioRXTimer = new cMessage("radioUpdate-RXTimer");
    radioRXTimer->addPar("windowIndex") = 0;
    retransmissionModel = (RetransmissionModel) par("retransmissionModel").intValue();


    listeningWindowStart1 = par("listeningWindowStart1").intValue();
    listeningWindowStart2 = par("listeningWindowStart2").intValue();
    listeningWindowDuration1 = par("listeningWindowDuration1").intValue();
    listeningWindowDuration2 = par("listeningWindowDuration2").intValue();
    listeningWindowRX1 = par("listeningWindowRX1").doubleValue();
    listeningWindowRX2 = par("listeningWindowRX2").doubleValue();
    listeningWindowRXR = par("listeningWindowRXR").doubleValue();
    listeningWindowRXC = par("listeningWindowRXC").doubleValue();
    listeningWindowRXCR = par("listeningWindowRXCR").doubleValue();

    currentConsumptionRx = par("currentConsumptionRx").doubleValue();
    currentConsumptionTx = par("currentConsumptionTx").doubleValue();
    currentConsumptionIdle = par("currentConsumptionIdle").doubleValue();
    currentConsumptionWOR = par("currentConsumptionWOR").doubleValue();

    energyConsumptionRx = 0;
    energyConsumptionTx = 0;
    energyConsumptionIdle = 0;
    energyConsumptionWOR = 0;

    collisionCount = 0;

    busyUntil = -1;
    if (deviceType != DevType::GATEWAY && deviceType != DevType::RELAY) {
        timer_listeningWindowStart1 = new cMessage("listeningWindowStart1");
        timer_listeningWindowStart1->setKind(SelfMsgType::WINDOWTIMER);
        scheduleAt(simTime() + listeningWindowStart1 * 60, timer_listeningWindowStart1);

        timer_listeningWindowStop1 = new cMessage("listeningWindowStop1");
        timer_listeningWindowStop1->setKind(SelfMsgType::WINDOWTIMER);
        scheduleAt(simTime() + listeningWindowStart1 * 60 + listeningWindowDuration1 * 60, timer_listeningWindowStop1);

        timer_listeningWindowStart2 = new cMessage("listeningWindowStart2");
        timer_listeningWindowStart2->setKind(SelfMsgType::WINDOWTIMER);
        scheduleAt(simTime() + listeningWindowStart2 * 60, timer_listeningWindowStart2);

        timer_listeningWindowStop2 = new cMessage("listeningWindowStop2");
        timer_listeningWindowStop2->setKind(SelfMsgType::WINDOWTIMER);
        scheduleAt(simTime() + listeningWindowStart2 * 60 + listeningWindowDuration2 * 60, timer_listeningWindowStop2);
    }

    WATCH(busyUntil);
    WATCH(radioActivity);
    WATCH(collisionCount);

    WATCH(energyConsumptionRx);
    WATCH(energyConsumptionTx);
    WATCH(energyConsumptionIdle);
    if (deviceType == DevType::RELAY)
        WATCH(energyConsumptionWOR);

    rssiOfCurrentReceive = sensitivity-1;
    currentRcvMessage = nullptr;
    radioUpdateReceive = nullptr;


    wakeupRelay = registerSignal("wakeupRelay");
    lastSpreadingFactor = 12;

    if(deviceType == DevType::RELAY)
        getSimulation()->getSystemModule()->subscribe("wakeupRelay", this);
}
void Radio::handleMessage(cMessage *msg)
{
    updateDaytime();
    //LoRaPacket *timeoutPkt;
    cGate *arrivalGate = msg->getArrivalGate();

    if (msg->isSelfMessage()) {
        switch (msg->getKind()) {
            case SelfMsgType::RADIOINIT:
                EV << simTime() << " RADIOINIT \n";
                updateRadio();
                delete msg;
                break;
            case SelfMsgType::WINDOWTIMER:
                EV << simTime() << " WINDOWTIMER \n";
                scheduleAt(simTime() + 24 * 3600, msg);
                updateRadio();
                break;
            case SelfMsgType::TXTIMER:
                EV << simTime() << " TXTIMER \n";
                getParentModule()->getDisplayString().setTagArg("r",0,"0");
                updateRadio();
                delete msg;
                if (deviceType != DevType::GATEWAY && !disableLAT)
                {
                    radioRXTimer->par("windowIndex") = 1;
                    radioRXTimer->setKind(SelfMsgType::RXC);
                    if (!radioRXTimer->isScheduled())
                        scheduleAt(simTime() + listeningWindowRXC, radioRXTimer);
                }
                disableLAT = false;
                break;
            case SelfMsgType::RXC:
                EV << simTime() << " RXC \n";
                if ((int) msg->par("windowIndex") == 1) {
                    EV << simTime() << " RXC windowIndex 1 \n";
                    updateRadioActivity(RadioActivity::LISTENING);
                    getParentModule()->getDisplayString().setTagArg("t",0,"Listening RX1");        //display bubble
                    msg->setKind(SelfMsgType::RX1);
                    scheduleAt(simTime() + listeningWindowRX1, msg);
                }
                if ((int) msg->par("windowIndex") == 2) {
                    EV << simTime() << " RXC windowIndex 2 \n";
                    updateRadioActivity(RadioActivity::LISTENING);
                    getParentModule()->getDisplayString().setTagArg("t",0,"Listening RX2");        //display bubble
                    msg->setKind(SelfMsgType::RX2);
                    scheduleAt(simTime() + listeningWindowRX2, msg);
                }
                if ((int) msg->par("windowIndex") == 3) {
                    EV << simTime() << " RXCR windowIndex 3 \n";
                    updateRadioActivity(RadioActivity::LISTENING);
                    getParentModule()->getDisplayString().setTagArg("t",0,"Listening RXR");        //display bubble
                    msg->setKind(SelfMsgType::RXR);
                    scheduleAt(simTime() + listeningWindowRXR, msg);
                }
                break;
            case SelfMsgType::RX1:
                EV << simTime() << " RX1 \n";
                getParentModule()->getDisplayString().setTagArg("t",0,"...");        //display bubble
                updateRadio();
                msg->par("windowIndex") = 2;
                msg->setKind(SelfMsgType::RXC);
                scheduleAt(simTime() + listeningWindowRXC, msg);
                break;
            case SelfMsgType::RX2:
                EV << simTime() << " RX2 \n";
                getParentModule()->getDisplayString().setTagArg("t",0,"...");        //display bubble
                updateRadio();
                if (listeningWindowRXRactive) {
                    msg->par("windowIndex") = 3;
                    msg->setKind(SelfMsgType::RXC);
                    scheduleAt(simTime() + listeningWindowRXCR, msg);
                }
                else {
                    msg->par("windowIndex") = 0;
                    if (sendQueue.size() > 0) {
                        sendToNetwork(NULL);
                    }
                }
                break;
            case SelfMsgType::RXR:
                EV << simTime() << " RXR \n";
                getParentModule()->getDisplayString().setTagArg("t",0,"");
                updateRadio();

                /*
                timeoutPkt = new LoRaPacket("timeout",0);
                timeoutPkt->setKind(MsgType::TIMEOUT);
                sendToNetwork(timeoutPkt);
                */

                if (sendQueue.size() > 0) {
                    sendToNetwork(NULL);
                }
                break;

            //Below case indicate that the message was correctly received. No disruption during transmission.
            case SelfMsgType::RXTIMER:
                if (currentRcvMessage != nullptr){
                    send(currentRcvMessage, "toLogic");
                    EV << "RADIO " << tmp_name << " t=" << simTime() << " rcv ok, sending to logic\n";

                    if (par("forceSF").intValue() == -1 && currentRcvMessage->getKind() == MsgType::ACK)        //ADR en
                    {
                        if (currentRcvMessage->hasPar("ADR+") && lastSpreadingFactor < 12)
                            lastSpreadingFactor++;
                        if (currentRcvMessage->hasPar("ADR-") && lastSpreadingFactor > 7)
                            lastSpreadingFactor--;
                    }
                }

                getParentModule()->getDisplayString().setTagArg("t",0,"");
                if (sendQueue.size() > 0) {
                    sendToNetwork(NULL);
                }
                else {
                    updateRadio();
                }



                currentRcvMessage = nullptr;
                delete msg;         //special object was created for the event so it's time to remove unused object
                break;

            //case SelfMsgType::

            default:
                EV << tmp_name << " default \n";
                delete msg;
                break;
        }
    }
    else if (strcmp(arrivalGate->getName(), "fromLogic") == 0) {
        if (msg->getKind() == MsgType::RESET_ADR)
        {
            if (lastSpreadingFactor < 12)
                lastSpreadingFactor++;
            delete msg;
            return;
        }


        if (dynamic_cast<LoRaPacket*>(msg) != nullptr)
        {
            if(msg->getKind() == MsgType::JOINREQUEST)                  //wyzeruj ADR
                lastSpreadingFactor = 12;
            sendToNetwork((LoRaPacket*)msg);
        }
        else
            //TODO: control messages to enable radio listen etc
            EV << "RADIO " << tmp_name << " Got unknown packet to send " << this->getParentModule()->getFullName() << " \n";

    }
    else if (strcmp(arrivalGate->getName(), "radioIn") == 0) {
        if (dynamic_cast<LoRaPacket*>(msg) != nullptr)
            receiveFromNetwork((LoRaPacket*)msg);
        else
            EV << "RADIO " << tmp_name << " t="<< simTime() <<" Got unknown packet via radio " << this->getParentModule()->getFullName() << " \n";
    }

}
void Radio::receiveSignal(cComponent *src, simsignal_t id, double value, cObject *details)
{
    if (id == wakeupRelay && deviceType == DevType::RELAY && retransmissionModel == RetransmissionModel::EP431)
    {
        // TODO - check closest relay
        if ((radioActivity != RadioActivity::RECEIVING) and (radioActivity != RadioActivity::TRANSMITTING))
        {
            Enter_Method("receiveSignal(cComponent *src, simsignal_t id, double value, cObject *details)");
            updateRadioActivity(RadioActivity::LISTENING);
            getParentModule()->getDisplayString().setTagArg("t",0,"Listening rly");               //unable to execute gui command (wrong thered?)
            scheduleAt(simTime() + value, new cMessage("Relay required", SelfMsgType::RXR));
            EV << " RADIO " << tmp_name << " t=" << simTime() << " " << "RL enabled to listen for ED\n";
        }
    }
}
void Radio::updateRadio()
{
    if (((radioActivity == RadioActivity::RECEIVING) or (radioActivity == RadioActivity::TRANSMITTING))
            and (simTime() < busyUntil)) {
        return;
    }
    switch (deviceType) {
        case DevType::GATEWAY:
            updateRadioActivity(RadioActivity::LISTENING);
            EV << " RADIO " << tmp_name << " t=" << simTime() << " " << "listening start" << "\n";
            getParentModule()->getDisplayString().setTagArg("t",0,"Listening");
            break;
        case DevType::RELAY:
            if (currentRcvMessage && currentRcvMessage->isWOR())
            {
                updateRadioActivity(RadioActivity::LISTENING);
                getParentModule()->getDisplayString().setTagArg("t",0,"Listening msg after WOR");
            }
            else
            {
                if (retransmissionModel == TS011)
                {
                    updateRadioActivity(RadioActivity::WOR);
                    getParentModule()->getDisplayString().setTagArg("t",0,"Listening WOR");
                }
                if (retransmissionModel == EP431)
                {
                    updateRadioActivity(RadioActivity::RADIOOFF);
                    getParentModule()->getDisplayString().setTagArg("t",0,"OFF");
                }
            }
            break;
        case DevType::ENDNODE:
            if (((dayTime >= listeningWindowStart1) and (dayTime < listeningWindowStart1 + listeningWindowDuration1))
                    or ((dayTime >= listeningWindowStart2)
                            and (dayTime < listeningWindowStart2 + listeningWindowDuration2))) {
                updateRadioActivity(RadioActivity::LISTENING);
                rssiOfCurrentReceive = sensitivity-1;

                EV << " RADIO " << tmp_name << " t=" << simTime() << " listening start" << "\n";
            }
            else {
                updateRadioActivity(RadioActivity::RADIOOFF);
                rssiOfCurrentReceive = sensitivity-1;

                EV << " RADIO " << tmp_name << " t=" << simTime() << " radio off" << "\n";
            }
            break;

        case DevType::IPBRIDGE:
            updateRadioActivity(RadioActivity::LISTENING);
            EV << " RADIO " << tmp_name << " t=" << simTime() << " " << "listening start" << "\n";
            getParentModule()->getDisplayString().setTagArg("t",0,"Listening");
            break;

        case DevType::DEVUNKNOWN:
            assert("Unknown module");
            break;
    }
}
void Radio::receiveFromNetwork(LoRaPacket *msg)
{
    float transmissionTime = 0;
    double rssi = 0;
    double realSensitivity;
    bool rcvOK = 0;

    if (msg->senderId != myKeeperIndex && deviceType == DevType::ENDNODE)
        return;

    //verify colisions with transmission:
    if (radioActivity == RadioActivity::TRANSMITTING)              //for gateway colistion is not possible
    {
        collisionCount++;
        this->getParentModule()->bubble("Tx collision");
        keeper->recordColision(this->myKeeperIndex, msg->getSenderModuleId());
        dropByCollision(msg);
        return;
    }

    //calc RSS based on distance from transmitter
    rssi = getRssi((LoRaPacket*)msg);

    //calculate sensitivity based on keeper distortion activity
    realSensitivity = sensitivity + sqrt(keeper->distortionLevel) + 2.5 * (12-msg->spreadingFactor);            //każde zmniejszenie SF powoduje 2,5db mniejszy rpóg detekcji

    //apply random variation based on signal observation
    double currentPL = normal(0, par("pathlosStd"));
    rssi += currentPL;

    msg->rssi = rssi;
    msg->rssi_margin = rssi - realSensitivity;

    EV << "RADIO " << tmp_name << " t=" << simTime() << " frame " << msg->getKind() << " RSSI = " << rssi << " SF=" << msg->spreadingFactor <<" PLR = "<<currentPL <<" ";

    if (radioActivity == RadioActivity::LISTENING) {
        if (rssi > realSensitivity) {
            EV << "\033[1;32m received ok \033[0m\n";
            rcvOK =  true;
        }
        else
        {
            EV << "\033[1;31m below threshold\033[0m " << realSensitivity << "\n";
        }
    }
    else if (radioActivity == RadioActivity::RECEIVING) {
        //New signal stronger than existing more than 6dBm. Current msg will be discarted new will start receiving
        if (rssi > rssiOfCurrentReceive + 6) {
            EV << "RADIO " << tmp_name << " t=" << simTime() << "Collision capture" << rssiOfCurrentReceive << "[dBm] new=" << rssi << "[dBm]\n";
            this->getParentModule()->bubble("Rx col. ex");
            dropByCollision(currentRcvMessage);
            currentRcvMessage = nullptr;
            rcvOK = true;
        }
        //New signal is 6dBm weaker than current receive. Current msg will not be affected.
        else if (rssi < rssiOfCurrentReceive - 6) {
            EV << "RADIO " << tmp_name << " t=" << simTime() << "Collision no break " << rssiOfCurrentReceive << "[dBm] new=" << rssi << "[dBm]\n";
            this->getParentModule()->bubble("Rx col. new");
            dropByCollision(msg);
            rcvOK =  false;
        }
        //RSSI of new frame simmilar to currently received. Both frames are broken
        else {
            EV << "RADIO " << tmp_name << " t=" << simTime() << "Collision both current=" << rssiOfCurrentReceive << "[dBm] new=" << rssi << "[dBm]\n";
            keeper->recordColision(this->myKeeperIndex, msg->getSenderModuleId());
            this->getParentModule()->bubble("Rx col. both");
            rcvOK = false;
            dropByCollision(currentRcvMessage);
            dropByCollision(msg);
            currentRcvMessage = nullptr;
        }
    }
    else if (radioActivity == RadioActivity::WOR || radioActivity == RadioActivity::EWOR) {
        if (rssi > realSensitivity && msg->isWOR())
        {
            this->getParentModule()->bubble("Woke up from WOR!");
            rcvOK = true;
        }
    }
    else
    {
        rcvOK = false;
    }

    if (rcvOK) {

        transmissionTime = ((LoRaPacket*)msg)->calcMsgDuration();
        EV << "RADIO " << tmp_name << " t=" << simTime() << " Msg receiving, len= " << transmissionTime << "[s]\n";
        getParentModule()->getDisplayString().setTagArg("t",0,"RECEIVING");

        //if (radioUpdateReceive != nullptr && radioUpdateReceive->isScheduled())                  //jak nie ustawione to wywala się
        //    cancelEvent(radioRXTimer);

        updateRadioActivity(RadioActivity::RECEIVING);

        busyUntil = simTime() + transmissionTime;
        //if (radioUpdateReceive == nullptr)
            radioUpdateReceive = new cMessage("radioUpdateReceive",SelfMsgType::RXTIMER);

        scheduleAt(busyUntil, radioUpdateReceive);
        currentRcvMessage = ((LoRaPacket*)msg)->dupLoRa();

        //this->energyConsumptionRx += transmissionTime * this->currentConsumptionRx;   //[mAs]
        //energyConsumptionVector.record(this->energyConsumptionRx);
    }

    keeper->recordFrameReception(this->myKeeperIndex, msg->getSenderModuleId(), rssi, rcvOK);
    rssiOfCurrentReceive = rssi;
}
void Radio::dropByCollision(LoRaPacket* lp)
{
    if (!lp) return; // safety

    lp->autodetectUplink();

    long devIdx = lp->par("KeeperIndex").longValue();
    long pktId  = lp->par("PacketId").longValue();

    EV << simTime() << " [Radio] Drop by COLLISION: dev=" << devIdx
       << " pkt=" << pktId << " dir=" << (lp->isUplink ? "UL" : "DL") << "\n";

    if (keeper) {
        if (lp->isUplink)
            keeper->reportUplinkCollision(devIdx, pktId);
        else
            keeper->reportDownlinkCollision(devIdx, pktId);
    }
    collisionCount++;
    delete lp; // kończymy żywot pakietu
}

void Radio::sendToNetwork(LoRaPacket *msg)
{
    if (msg == NULL) {
        if (sendQueue.size() > 0) {
            LoRaPacket *msgFromQueue = new LoRaPacket(sendQueue.front());
            directSendWithDelete(msgFromQueue);
            sendQueue.erase(sendQueue.begin());
        }
    }
    else {
       directSendWithDelete(msg);
    }
}
void Radio::directSendWithDelete(LoRaPacket *msg)
{
    double transmissionTime;

    if (keeper->returnRadioDevices().size() > 0 && dynamic_cast<LoRaPacket *>(msg) != nullptr)
    {
        LoRaPacket *job;
        job = (LoRaPacket*)msg;

        if (deviceType == DevType::ENDNODE)
            job->senderId = myKeeperIndex;

        cModule* en = getParentModule();                 // Radio jest submodułem Endnode
        job->senderPositionX = position.x;
        job->senderPositionY = position.y;
        job->transmissionPower = txPower;
        job->channel = 1; //TODO
        //job->spreadingFactor = par("forceSF").intValue();
        if (job->spreadingFactor == -1)     //means ADR enabled
            job->spreadingFactor = lastSpreadingFactor;

        //int msgSize = job->length;
        //int spreadingfactor = job->spreadingFactor;

        updateRadioActivity(RadioActivity::TRANSMITTING);
        transmissionTime = job->calcMsgDuration(); //transmissionTimeInSec(msgSize, spreadingfactor);

        busyUntil = simTime() + transmissionTime;

        cMessage *radioUpdateTransmission = new cMessage("radioUpdate-transmission");       //to chyba nie powinno tak wygl�da�. Pwinna by� jedna wiadomo�� w k�ko wysy�ana jak mechanizm self-frame
        radioUpdateTransmission->setKind(SelfMsgType::TXTIMER);
        if (msg->getKind() == MsgType::MSGWOR)
            disableLAT = true;

        //this->energyConsumptionTx += transmissionTime * this->currentConsumptionTx;   //[mAs]     //replaced with updateRadioActivity()
        //this->par("energyConsumption").setDoubleValue(energyConsumptionTx + energyConsumptionRx);

        scheduleAt(busyUntil, radioUpdateTransmission);
        EV << "RADIO" << tmp_name << " t=" << simTime() << " Sending radio frame "<< job->GetPacketName() << "\n";

        getParentModule()->getDisplayString().setTagArg("r",0,"200");
    }
    for (Node &device : keeper->returnRadioDevices()) // access by reference to avoid copying
    {

        //cModule *target = getParentModule()->getParentModule()->getSubmodule(device.name.c_str(), device.index);
        if (device.radio == nullptr)
            continue;

        if (device.radio == this)       //nie wysyłaj do siebie
            continue;

        // filter: skip devices whose radio is OFF or UNKNOWN
        if (device.radio->isOffOrUnknown()) {
            continue;
        }

        sendDirect(msg->dupLoRa(), device.radio->getParentModule(), "radioIn");

        /*
        std::string s = device.radio->getParentModule()->getFullName();
        if (deviceType == DevType::ENDNODE)
        {
            if (s.rfind("endnode", 0) == std::string::npos){
                sendDirect(msg->dupLoRa(), device.radio->getParentModule(), "radioIn");
            }
        } else {
            sendDirect(msg->dupLoRa(), device.radio->getParentModule(), "radioIn");
        }*/
        //if (((Radio*)target)->isListening())
    }
    keeper->recordFrameTransmission(myKeeperIndex, msg->calcMsgDuration());

    delete (LoRaPacket*)msg;
}
bool Radio::isReceivedCorrectly(cMessage *msg, double rssi)
{
    //delete msg;
    return false;

}

bool Radio::isListening()
{
    if (deviceType == DevType::GATEWAY)
        return true;

    if (deviceType == DevType::RELAY)
        return true;

    return (radioActivity == RadioActivity::LISTENING);
}
double Radio::getRssi(LoRaPacket *msg)
{
    //position.x = atof(getParentModule()->getDisplayString().getTagArg("p", 0));
    //position.y = atof(getParentModule()->getDisplayString().getTagArg("p", 1));

    double distance = sqrt(
            pow(position.x - msg->senderPositionX, 2) + pow(position.y - msg->senderPositionY, 2));

     EV << "RADIO" << tmp_name << " t=" << simTime() <<" Verify receive from: " << msg->senderPositionX << ", " << msg->senderPositionY << " distance: " << distance << "m\n";

    return calculateReceivedPower(msg->transmissionPower, distance);
}
double Radio::calculateReceivedPower(float pSend, double distance) //Okumura lite
{
    if (int(distance) == 0) {
        return pSend;
    }
    float K1 = 127.5;
    float K2 = 35.2;
    float PL = K1 + K2 * log10(distance / 500.0);           //deeper distortion affects RSSI

    float pRec = pSend - PL;
    if (pRec > pSend)
        pRec = pSend;

    return pRec;

}

void Radio::updateDaytime()
{
    int64_t timeInMinutesFromTheBeginning = simTime().inUnit(SIMTIME_S) / 60;
    dayTime = timeInMinutesFromTheBeginning % (60 * 24);
}

void Radio::updateRadioActivity(RadioActivity::ra newRA)
{
    double duration = (simTime() - lastRadioActivityChange).dbl();

    switch (radioActivity)
    {
        case RadioActivity::UNKNOWN:    //first init only
            break;

        case RadioActivity::RADIOOFF:
        case RadioActivity::TIMER:
            energyConsumptionIdle += duration * currentConsumptionIdle;
            break;

        case RadioActivity::RECEIVING:
        case RadioActivity::LISTENING:
            energyConsumptionRx += duration * currentConsumptionRx;
            break;


        case RadioActivity::EWOR:
        case RadioActivity::WOR:
            energyConsumptionWOR += duration * currentConsumptionWOR;
            break;

        case RadioActivity::TRANSMITTING:
            energyConsumptionTx += duration * currentConsumptionTx;
            break;
    }

    radioActivity = newRA;
    lastRadioActivityChange = simTime();

    keeper->recordEnergyConsumption(myKeeperIndex, energyConsumptionIdle + energyConsumptionRx + energyConsumptionTx + energyConsumptionWOR);
}

/*
 * Offset current [uA]
261,20  FS12
150,65
70,51
41,56
25,68
16,90   SF7
 *
 */


bool Radio::isOffOrUnknown() const noexcept {
    return (radioActivity == RadioActivity::RADIOOFF)
        || (radioActivity == RadioActivity::UNKNOWN);
}

void Radio::finish()
{
    recordScalar("Energy consumption total Tx", this->energyConsumptionTx);
    recordScalar("Energy consumption total Rx", this->energyConsumptionRx);
    recordScalar("Energy consumption total Idle", this->energyConsumptionIdle);
    recordScalar("Energy consumption total WOR", this->energyConsumptionWOR);
}

static inline double readCoord(cModule* m, const char* name, int tagIndex) {
    if (m->hasPar(name))               // preferuj parametry
        return m->par(name).doubleValue();
    // fallback: spróbuj display string "p"
    const char* s = m->getDisplayString().getTagArg("p", tagIndex);
    return (s && *s) ? atof(s) : 0.0;
}

}
;
// namespace
