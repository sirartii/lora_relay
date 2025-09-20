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
#include <algorithm>
#include <cmath>
#include "LoRaPacket.h"

namespace opp_lora_relay_official {

Define_Module(LogicEndnode);

LogicEndnode::LogicEndnode()
{
    // timerMessageWindow1/2 tworzymy w initialize()
}

LogicEndnode::~LogicEndnode()
{
    if (timerMessageWindow1) cancelAndDelete(timerMessageWindow1);
    if (timerMessageWindow2) cancelAndDelete(timerMessageWindow2);
}

void LogicEndnode::initialize()
{
    updateDaytime();

    // Parametry okien (minuty od północy / czas trwania w minutach)
    transmittingWindowStart1    = par("transmittingWindowStart1").intValue();
    transmittingWindowStart2    = par("transmittingWindowStart2").intValue();
    transmittingWindowDuration1 = par("transmittingWindowDuration1").intValue();
    transmittingWindowDuration2 = par("transmittingWindowDuration2").intValue();

    // Nowe parametry konfigurowalne z ini
    windowsPerDay      = hasPar("windowsPerDay")      ? par("windowsPerDay").intValue()      : 2;
    framesPerWindowMin = hasPar("framesPerWindowMin") ? par("framesPerWindowMin").intValue() : 1;
    framesPerWindowMax = hasPar("framesPerWindowMax") ? par("framesPerWindowMax").intValue() : 4;
    if (framesPerWindowMax < framesPerWindowMin) std::swap(framesPerWindowMin, framesPerWindowMax);
    if (framesPerWindowMin < 1) framesPerWindowMin = 1;

    // Losowa liczba ramek do wysłania w bieżącym cyklu dla okna #1 i #2
    framesRemainingWin1 = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;
    framesRemainingWin2 = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;

    // Początek dzisiejszej doby (dokładnie, w minutach)
    simtime_t dayStart = simTime() - SimTime(dayTime*60000, SIMTIME_MS);

    // Okno #1: start + losowy offset w ms
    timerMessageWindow1 = new cMessage("timerMessageWindow1");
    timerMessageWindow1->setKind(SelfMsgType::TXWINDOW1);
    {
        simtime_t start1 = dayStart + SimTime(transmittingWindowStart1*60000, SIMTIME_MS);
        long long dur1ms = 1LL * transmittingWindowDuration1 * 60LL * 1000LL;
        long long off1ms = dur1ms > 0 ? intrand((int)dur1ms) : 0;
        simtime_t t1     = start1 + SimTime(off1ms, SIMTIME_MS);
        if (t1 < simTime()) t1 += SimTime(24 * 60 * 60000, SIMTIME_MS);
        scheduleAt(t1, timerMessageWindow1);
    }

    // Okno #2 (opcjonalnie)
    timerMessageWindow2 = nullptr;
    if (windowsPerDay >= 2) {
        timerMessageWindow2 = new cMessage("timerMessageWindow2");
        timerMessageWindow2->setKind(SelfMsgType::TXWINDOW2);
        simtime_t start2 = dayStart + SimTime(transmittingWindowStart2 * 60000, SIMTIME_MS);
        long long dur2ms = 1LL * transmittingWindowDuration2 * 60LL * 1000LL;
        long long off2ms = dur2ms > 0 ? intrand((int)dur2ms) : 0;
        simtime_t t2     = start2 + SimTime(off2ms, SIMTIME_MS);
        if (t2 < simTime()) t2 += SimTime(24 * 60 * 60000, SIMTIME_MS);
        scheduleAt(t2, timerMessageWindow2);
    }

    /*
        int index = this->getParentModule()->getIndex();
        int posX = 300 + (index % 5) * 100;
        int posY = 100 + (index / 5) * 120;

        if (index == 18 or index == 19)
            posX += 300;

        if (index == 0 ||index == 5)
            posX += 800;*/


        //this->getParentModule()->getDisplayString().setTagArg("p", 0, posX);
        //this->getParentModule()->getDisplayString().setTagArg("p", 1, posY);
        //this->getParentModule()->getDisplayString().setTagArg("p", 2, "");
        //this->getParentModule()->getDisplayString().setTagArg("p", 3, "");
        //this->getParentModule()->getDisplayString().setTagArg("p", 4, "");

        //getAncestorPar("xpos").setDoubleValue(posX);
        //getAncestorPar("ypos").setDoubleValue(posY);

        endDeviceState = EndDeviceState::OFFLINE;         //this must stay, otherwise below will throw an exception
        updateLogicalState(EndDeviceState::OFFLINE);

        retransmissionModel = (RetransmissionModel) par("retransmissionModel").intValue();
        RTCdrift = par("clockDriftMax").doubleValue() / 1000000.0;  //ppm's in config

        keeper = check_and_cast<Keeper*>(getParentModule()->getParentModule()->getSubmodule("keeper"));
        myKeeperIndex = keeper->registerEndnode();

        lastACK = 0;
        waitingResponse = 0;
        missedResponse = 0;
        packetCounter = 0;
        MsgAfterWOR = 0;

        WATCH(lastACK);
        WATCH(packetCounter);

        wakeupRelay = registerSignal("wakeupRelay");
        offlineMax = 0;
        offlineTotal = 0;
        lastOnline = simTime();
}

void LogicEndnode::handleMessage(cMessage *msg)
{
    updateDaytime();

    if (msg->isSelfMessage()) {

        // wejdziemy tu tylko, gdy oczekiwaliśmy odpowiedzi i minął timeout
        if (msg->getKind() == SelfMsgType::RXTIMER)
        {
            if (!waitingResponse)
                return;

            waitingResponse = 0;
            missedResponse++;

            if (missedResponse > 5)
            {
                missedResponse = 0;
                //updateLogicalState(EndDeviceState::OFFLINE);
                updateLogicalState(EndDeviceState::JOIN_RELAY);
            }
            cMessage* ra = new cMessage("ResetADR",MsgType::RESET_ADR);
            send(ra, "logicOut");
            return;
        }

        // Koniec transmisji (animacja/GUI)
        if (msg->getKind() == SelfMsgType::TXTIMER)
        {
            this->getParentModule()->getDisplayString().setTagArg("r",0,"0");
            return;
        }

        // Po WOR nadajemy zasadniczą ramkę
        if (msg->getKind() == SelfMsgType::TXREPEAT) {
            MsgAfterWOR = 1;
        }

        if (msg->getKind() == SelfMsgType::TXWINDOW1) {
            // 1) Początek bieżącej doby: PODŁOGA do 24 h (nie używamy 'dayTime')
            static const long long DAY_MS = 24LL * 60LL * 60000LL;
            const long long nowMs  = simTime().inUnit(SIMTIME_MS);
            const long long day0Ms = (nowMs / DAY_MS) * DAY_MS;
            simtime_t dayStart     = SimTime(day0Ms, SIMTIME_MS);

            simtime_t start = dayStart + SimTime(1LL * transmittingWindowStart1 * 60000LL, SIMTIME_MS);
            simtime_t end   = start   + SimTime(1LL * transmittingWindowDuration1 * 60000LL, SIMTIME_MS);

            const long long durMs = (end - start).inUnit(SIMTIME_MS);
            const int N = framesPerWindowMin; // (jak pisałeś: min == max)

            // 2) Przed startem / po oknie lub brak ramek -> ustaw pierwszy slot i WYJDŹ
            if (simTime() < start || simTime() >= end || framesRemainingWin1 <= 0) {
                if (framesRemainingWin1 <= 0)
                    framesRemainingWin1 = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;

                // dziś, jeśli jeszcze przed startem; inaczej jutro
                simtime_t base = (simTime() < start)
                               ? start
                               : (dayStart + SimTime(DAY_MS, SIMTIME_MS)
                                         + SimTime(1LL * transmittingWindowStart1 * 60000LL, SIMTIME_MS));

                if (framesRemainingWin1 > 0 && durMs > 0) {
                    long long slot = std::max(1LL, durMs / framesRemainingWin1);
                    long long phi  = intrand((int)slot);   // faza na całe okno
                    base += SimTime(phi, SIMTIME_MS);
                }
                if (base <= simTime()) base = simTime() + SimTime(1, SIMTIME_MS); // <-- zabezpieczenie
                scheduleAt(base, msg);
                return;
            }

            // 3) Jesteśmy w oknie: wyślij TERAZ i zaplanuj kolejną w następnym slocie
            framesRemainingWin1--;

            if (durMs > 0 && N > 0) {
                long long slot = std::max(1LL, durMs / N);
                long long elapsed = (simTime() - start).inUnit(SIMTIME_MS);
                long long k   = elapsed / slot;
                long long phi = elapsed - k * slot; // „faza” = (now - start) % slot

                if (framesRemainingWin1 > 0) {
                    simtime_t tnext = start + SimTime((k + 1) * slot + phi, SIMTIME_MS); // = now + slot (w msowej siatce)
                    if (tnext >= end) {
                        // brak miejsca -> pierwszy slot jutrzejszego okna
                        framesRemainingWin1 = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;
                        tnext = dayStart + SimTime(DAY_MS, SIMTIME_MS)
                                       + SimTime(1LL * transmittingWindowStart1 * 60000LL, SIMTIME_MS);
                        if (framesRemainingWin1 > 0) {
                            long long slot2 = std::max(1LL, durMs / framesRemainingWin1);
                            tnext += SimTime(intrand((int)slot2), SIMTIME_MS);
                        }
                    }
                    if (tnext <= simTime()) tnext = simTime() + SimTime(1, SIMTIME_MS); // <-- zabezpieczenie
                    scheduleAt(tnext, msg);
                    // UWAGA: brak 'return' – poniżej wysyłasz bieżącą ramkę
                } else {
                    // to była ostatnia ramka w tym oknie → ustaw pierwszy slot na jutro
                    framesRemainingWin1 = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;
                    simtime_t tnext = dayStart + SimTime(DAY_MS, SIMTIME_MS)
                                              + SimTime(1LL * transmittingWindowStart1 * 60000LL, SIMTIME_MS);
                    if (framesRemainingWin1 > 0) {
                        long long slot2 = std::max(1LL, durMs / framesRemainingWin1);
                        tnext += SimTime(intrand((int)slot2), SIMTIME_MS);
                    }
                    if (tnext <= simTime()) tnext = simTime() + SimTime(1, SIMTIME_MS); // <-- zabezpieczenie
                    scheduleAt(tnext, msg);
                    // bez 'return' – najpierw wyślij bieżącą ramkę
                }
            }

            // >>> tutaj pozostaje Twój kod, który faktycznie wysyła ramkę "TERAZ"
        }


        // --- Okno nadawcze #2 ------------------------------------------------
        if (msg->getKind() == SelfMsgType::TXWINDOW2) {
            static const long long DAY_MS = 24LL * 60LL * 60000LL;
            const long long nowMs  = simTime().inUnit(SIMTIME_MS);
            const long long day0Ms = (nowMs / DAY_MS) * DAY_MS;
            simtime_t dayStart     = SimTime(day0Ms, SIMTIME_MS);

            simtime_t start = dayStart + SimTime(1LL * transmittingWindowStart2 * 60000LL, SIMTIME_MS);
            simtime_t end   = start   + SimTime(1LL * transmittingWindowDuration2 * 60000LL, SIMTIME_MS);

            const long long durMs = (end - start).inUnit(SIMTIME_MS);
            const int N = framesPerWindowMin;

            if (simTime() < start || simTime() >= end || framesRemainingWin2 <= 0) {
                if (framesRemainingWin2 <= 0)
                    framesRemainingWin2 = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;

                simtime_t base = (simTime() < start)
                               ? start
                               : (dayStart + SimTime(DAY_MS, SIMTIME_MS)
                                         + SimTime(1LL * transmittingWindowStart2 * 60000LL, SIMTIME_MS));

                if (framesRemainingWin2 > 0 && durMs > 0) {
                    long long slot = std::max(1LL, durMs / framesRemainingWin2);
                    long long phi  = intrand((int)slot);
                    base += SimTime(phi, SIMTIME_MS);
                }
                if (base <= simTime()) base = simTime() + SimTime(1, SIMTIME_MS);
                scheduleAt(base, msg);
                return;
            }

            framesRemainingWin2--;

            if (durMs > 0 && N > 0) {
                long long slot = std::max(1LL, durMs / N);
                long long elapsed = (simTime() - start).inUnit(SIMTIME_MS);
                long long k   = elapsed / slot;
                long long phi = elapsed - k * slot;

                if (framesRemainingWin2 > 0) {
                    simtime_t tnext = start + SimTime((k + 1) * slot + phi, SIMTIME_MS);
                    if (tnext >= end) {
                        framesRemainingWin2 = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;
                        tnext = dayStart + SimTime(DAY_MS, SIMTIME_MS)
                                       + SimTime(1LL * transmittingWindowStart2 * 60000LL, SIMTIME_MS);
                        if (framesRemainingWin2 > 0) {
                            long long slot2 = std::max(1LL, durMs / framesRemainingWin2);
                            tnext += SimTime(intrand((int)slot2), SIMTIME_MS);
                        }
                    }
                    if (tnext <= simTime()) tnext = simTime() + SimTime(1, SIMTIME_MS);
                    scheduleAt(tnext, msg);
                } else {
                    framesRemainingWin2 = intrand(framesPerWindowMax - framesPerWindowMin + 1) + framesPerWindowMin;
                    simtime_t tnext = dayStart + SimTime(DAY_MS, SIMTIME_MS)
                                              + SimTime(1LL * transmittingWindowStart2 * 60000LL, SIMTIME_MS);
                    if (framesRemainingWin2 > 0) {
                        long long slot2 = std::max(1LL, durMs / framesRemainingWin2);
                        tnext += SimTime(intrand((int)slot2), SIMTIME_MS);
                    }
                    if (tnext <= simTime()) tnext = simTime() + SimTime(1, SIMTIME_MS);
                    scheduleAt(tnext, msg);
                }
            }

            // >>> poniżej: wysyłka ramki "TERAZ"
        }


        // --- Emisja ramki (logika stanu urządzenia) --------------------------

        cModule *parent = getParentModule();
        LoRaPacket *job = new LoRaPacket(parent->getFullName(), 51);

        job->id = packetCounter;
        job->ackRequired = (packetCounter % 5) == 0 || lastACK > 0; // co 5-ta lub gdy poprzedni ACK nie dotarł
        packetCounter++;

        switch (endDeviceState){

            case EndDeviceState::OFFLINE:
            case EndDeviceState::JOIN_DIRECT:
                job->setKind(MsgType::JOINREQUEST);
                job->setName("JOIN DIRECT");
                job->ackRequired = true;
                job->relayRequired = false;
                updateLogicalState(EndDeviceState::JOIN_DIRECT);
                job->length = 15;
                if (packetCounter > par("missedPacketsToSearchRelay").intValue())
                {
                    packetCounter = 0;
                    updateLogicalState(EndDeviceState::JOIN_RELAY);
                }
                break;

            // TS011: WOR -> JOIN; EN038: pojedynczy JOIN_RELAY
            case EndDeviceState::JOIN_RELAY:
                if (MsgAfterWOR)
                {
                    job->setName("JOIN RELAY");
                    job->setKind(MsgType::JOINREQUEST);
                    MsgAfterWOR = 0;
                    job->ackRequired = true;
                    job->relayRequired = true;
                    job->length = RELAY_JOIN_LENGTH;
                }
                else
                {
                    if (retransmissionModel == RetransmissionModel::EP431)
                    {
                        cMessage *rxWORMsg = msg->dup();
                        rxWORMsg->setKind(SelfMsgType::TXREPEAT);

                        double windowsize = SIMTIME_DBL(simTime() - lastACKTime) * RTCdrift;
                        long long wsMs = (long long) llround(std::max(0.0, windowsize) * 1000.0);
                        scheduleAt(simTime() + SimTime(wsMs, SIMTIME_MS), rxWORMsg);
                        emit(wakeupRelay, windowsize*2, this);
                        return; // nie wysyłamy teraz ramki radiowej
                    }
                    else
                    {
                        job->setName("WOR");
                        job->setKind(MsgType::MSGWOR);
                        job->length = WOR_DATA_LENGTH;
                        job->ackRequired = false;

                        // zaplanuj JOIN po WOR
                        cMessage *rxWORMsg = msg->dup();
                        rxWORMsg->setKind(SelfMsgType::TXREPEAT);
                        scheduleAt(simTime() + WOR_DATA_DELAY, rxWORMsg);
                    }
                }
                break;

            case EndDeviceState::ONLINE_DIRECT:
                job->setKind(MsgType::UPLINK);
                if (job->ackRequired) job->setName("UPLINK_W_ACK");
                else                  job->setName("UPLINK");

                if (lastACK > par("missedPacketsToSearchRelay").intValue())
                {
                    updateLogicalState(EndDeviceState::OFFLINE);
                    lastACK = 0;
                    packetCounter = 0;
                }
                job->length = 51; // payload przykładowy
                break;

            case EndDeviceState::ONLINE_RELAYED:
                if (MsgAfterWOR)
                {
                    job->setKind(MsgType::UPLINK);
                    job->relayRequired = 1;
                    if (job->ackRequired) job->setName("UPLINK_W_ACK");
                    else                  job->setName("UPLINK");

                    if (lastACK > par("missedPacketsToSearchRelay").intValue())
                    {
                        updateLogicalState(EndDeviceState::OFFLINE);
                        lastACK = 0;
                        packetCounter = 0;
                    }
                    job->length = 51;
                    MsgAfterWOR = 0;
                }
                else
                {
                    if (retransmissionModel == RetransmissionModel::EP431)
                    {
                        cMessage *rxWORMsg = msg->dup();
                        rxWORMsg->setKind(SelfMsgType::TXREPEAT);

                        double windowsize = SIMTIME_DBL(simTime() - lastACKTime) * RTCdrift;
                        long long wsMs = (long long) llround(std::max(0.0, windowsize) * 1000.0);
                        scheduleAt(simTime() + SimTime(wsMs, SIMTIME_MS), rxWORMsg);
                        emit(wakeupRelay, windowsize*2, this);
                        return;
                    }
                    else
                    {
                        job->setName("WOR");
                        job->setKind(MsgType::MSGWOR);
                        job->length = WOR_DATA_LENGTH;
                        job->ackRequired = false;

                        cMessage *rxWORMsg = msg->dup();
                        rxWORMsg->setKind(SelfMsgType::TXREPEAT);
                        scheduleAt(simTime() + WOR_DATA_DELAY, rxWORMsg);
                    }
                }
                break;

            default:
                break;
        }

        // Ustaw parametry radiowe i metadane
        job->senderPositionX   = getParentModule()->par("xpos").doubleValue();
        job->senderPositionY   = getParentModule()->par("ypos").doubleValue();
        job->transmissionPower = 14;
        job->channel           = 1;   // TODO
        job->addPar("KeeperIndex") = myKeeperIndex;
        job->addPar("PacketId")    = packetCounter;

        int sfParam = par("forceSF").intValue();
        if (endDeviceState == EndDeviceState::JOIN_RELAY ||
            endDeviceState == EndDeviceState::ONLINE_RELAYED ||
            job->relayRequired)                      // dodatkowe zabezpieczenie
        {
            job->spreadingFactor = 9;
        }
        else {
            job->spreadingFactor = sfParam;
        }

        waitingResponse = job->ackRequired;

        // Bardzo ważne dla prawidłowej klasyfikacji kolizji/statystyk
        job->isUplink = true;

        send(job, "logicOut");
        keeper->recordMessageTransmission(myKeeperIndex, packetCounter, true);

        if (waitingResponse)
        {
            lastACK++;
            cMessage *rxTimerMsg = msg->dup();
            rxTimerMsg->setKind(SelfMsgType::RXTIMER);
            scheduleAt(simTime() + REPLAY_TIMEOUT, rxTimerMsg);
        }
    }
    else {
        // Przetwarzanie DL
        if (msg->arrivedOn("logicIn")) {
            LoRaPacket *msgLoRa = dynamic_cast<LoRaPacket*>(msg);

            // Zarejestruj dostarczenie DL/ACK dla statystyk dziennych
            if (msgLoRa && !msgLoRa->isUplink) {
                long devIdx = msgLoRa->par("KeeperIndex").longValue();
                long pktId  = msgLoRa->par("PacketId").longValue();
                keeper->recordDownlinkReception(devIdx, pktId); // istniejąca u Ciebie metoda
            }

            missedResponse = 0;

            if (msgLoRa->getKind() == MsgType::JOINACCEPT)
            {
                if (endDeviceState == EndDeviceState::JOIN_RELAY)
                    updateLogicalState(EndDeviceState::ONLINE_RELAYED);
                else if (endDeviceState == EndDeviceState::JOIN_DIRECT)
                    updateLogicalState(EndDeviceState::ONLINE_DIRECT);

                lastACK = 0;
                lastACKTime = simTime();
                waitingResponse = 0;
            }

            if (msgLoRa->getKind() == MsgType::ACK)
            {
                lastACK = 0;
                lastACKTime = simTime();
                waitingResponse = 0;
            }

            if (msgLoRa->getKind() == MsgType::TIMEOUT)
            {
                updateLogicalState(EndDeviceState::OFFLINE);
            }

            delete msg;
        }
    }
}

void LogicEndnode::updateDaytime()
{
    // dayTime w MINUTACH w obrębie doby
    int64_t timeInMinutesFromStart = simTime().inUnit(SIMTIME_S) / 60;
    dayTime = timeInMinutesFromStart % (60 * 24);
}

void LogicEndnode::recordOnline()
{
    double offlineTime = (simTime() - lastOnline).dbl();
    offlineMax = fmax(offlineMax, offlineTime);
    offlineTotal += offlineTime;

    keeper->recordOfflineTime(myKeeperIndex, offlineTime);
}

void LogicEndnode::updateLogicalState(EndDeviceState newState)
{
    if (this->endDeviceState != newState)
        keeper->updateEndnodeState(myKeeperIndex, newState);

    this->endDeviceState = newState;
    cDisplayString &dString = this->getParentModule()->getDisplayString();

    switch ((short)newState)
    {
        case EndDeviceState::OFFLINE:
            dString.setTagArg("i2",0,"status/excl2");
            lastOnline = simTime();
            break;

        case EndDeviceState::JOIN_DIRECT:
            dString.setTagArg("i2",0,"status/question");
            break;

        case EndDeviceState::JOIN_RELAY:
            dString.setTagArg("i2",0,"status/bulb");
            break;

        case EndDeviceState::ONLINE_DIRECT:
            dString.setTagArg("i2",0,"status/green");
            recordOnline();
            break;

        case EndDeviceState::ONLINE_RELAYED:
            dString.setTagArg("i2",0,"status/yellow");
            recordOnline();
            break;
    }
}

void LogicEndnode::handleParameterChange(const char *parname)
{
    // TODO: ewentualne przeładowanie parametrów w runtime
    EV << "New Parameters: " << (parname ? parname : "<null>");
    return;
}

void LogicEndnode::finish()
{
    recordOnline();
}

} // namespace opp_lora_relay_official
