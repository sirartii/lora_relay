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

#include "LNS.h"
#include "common.h"
#include "LoRaPacket.h"


namespace opp_lora_relay_official {

Define_Module(LNS);

// ================== Pomocnicze ==================
double LNS::responseTime() const {
    // jak w Twoim kodzie
    return par("listeningWindowRXC").doubleValue()
         + (par("listeningWindowRX1").doubleValue() / 2.0);
}

// ================== cSimpleModule ==================
void LNS::initialize()
{
    keeper = check_and_cast<Keeper*>(getParentModule()->getSubmodule("keeper"));

    if (hasPar("dedupHoldoff"))
        dedupHoldoff = par("dedupHoldoff");
}

void LNS::finish()
{
    // *** DEDUP ***: porządki
    for (auto &kv : dedup) {
        auto &e = kv.second;
        if (e.timer) cancelAndDelete(e.timer);
        if (e.representative) delete e.representative;
    }
    dedup.clear();
}

void LNS::handleMessage(cMessage *msg)
{
    // 1) *** DEDUP TIMER *** (musi być PRZED gałęzią "zwykły self-message")
    if (msg->isSelfMessage() && msg->getKind() == KIND_DEDUP_TIMER) {
        onDedupTimer(msg);
        return;
    }

    LoRaPacket *msgLoRa = dynamic_cast<LoRaPacket*>(msg);

    // pora wygenerowa� i wys�a� pakiet LoRa
    if (msg->isSelfMessage())
    {
        LoRaPacket *pck = new LoRaPacket(msgLoRa);

        pck->length = 16;
        //pck->stKind();                //Przypisanie w kostruktorze. JoinAccept   Command  ACK
        pck->isUplink = false;
        pck->relayRequired = msgLoRa->isRelayed;
        pck->isRelayed = false;
        pck->transmissionPower = 14;
        pck->spreadingFactor = 12;

        //pck->id = 0;

        send(pck, "json$o", msgLoRa->gateLNS);
        keeper->recordMessageTransmission(0, ++packetCounter, false);

        EV << simTime() << " LNS Transmitting \n";

        delete msg;
        return;
    }

    if (dynamic_cast<LoRaPacket *>(msg) == nullptr )
    {
        EV << simTime() << " LNS Got incorrect LoRa Packet " << " \n";
        delete msg;
        return;
    }

    if (msg->arrivedOn("json$i")) {

        int inGate = msg->getArrivalGate()->getIndex();
        auto *ul = check_and_cast<LoRaPacket*>(msg);

        onUplinkArrived(ul, inGate);  // *** DEDUP *** (zatrzymuje lub „wchłania” kopie)
        return;





        //below we are certain that message is LoRa Packtet comming from outside

        double responseTime = par("listeningWindowRXC").doubleValue() + (par("listeningWindowRX1").doubleValue() / 2);
        LoRaPacket *response = ((LoRaPacket*)msg)->dupLoRa();
        response->gateLNS = msg->getArrivalGate()->getIndex();


        if (msg->getKind() == (short)MsgType::JOINREQUEST)
        {
            response->setKind(MsgType::JOINACCEPT);
            response->setName("JOIN ACCEPT");

            scheduleAt(simTime() + responseTime, response);

            EV << simTime() << " LNS Sending join accept in " << responseTime <<"s\n";
        }

        else if (msg->getKind() == JOINREQUEST && msgLoRa->isRelayed == 1)
        {
            response->setKind(MsgType::JOINACCEPT);
            response->isRelayed = 1;
            response->setName("RELAY JOIN ACCEPT");

            response->length = 14;
            scheduleAt(simTime() + responseTime, response);


            //EV << simTime() << " LNS Sending relay join accept to in " << (par("listeningWindowRX1").doubleValue() * 1,5) <<"\n";
        }

        else if (msg->getKind() == (short)MsgType::UPLINK)
        {
            if (response->ackRequired)
            {
                response->setKind(MsgType::ACK);
                response->setName("ACK");

                response->relayRequired = ((LoRaPacket*)msg)->isRelayed;
                response->isRelayed = 0;

                response->length = 15;

                if (uniform(0,100) > 95)
                    response->length = 50;

                scheduleAt(simTime() + responseTime, response);
            }
        }

        else if (msg->getKind() == (short)MsgType::DOWNLINK)
        {
            //not for LNS.
            //return;
        }

        keeper->recordMessageReception(msg->par("KeeperIndex"), msg->par("PacketId"));

        delete msg;
    }
}

void LNS::onUplinkArrived(LoRaPacket* ul, int inGateIdx)
{
    // Klucze duplikatu bierzemy z parametrów, których już używasz w Keeperze
    long devIdx = ul->par("KeeperIndex").longValue();  // identyfikator EndDevice
    long pktId  = ul->par("PacketId").longValue();     // identyfikator pakietu
    long long K = makeKey(devIdx, pktId);

    // RSSI z pakietu (DOPASUJ nazwę pola, jeśli u Ciebie inna)
    double rssi = ul->rssi; // <-- DOPASUJ NAZWĘ POLA jeżeli trzeba (np. ul->rssiDbm)

    // Aktualizacja średniej RSSI dla (urządzenie, wejście)
    avgRssiByDevGate[devIdx][inGateIdx].add(rssi);

    auto it = dedup.find(K);
    if (it == dedup.end()) {
        // Pierwsze nadejście — twórz entry i timer hold-off
        DedupEntry e;
        e.devIdx = devIdx;
        e.pktId  = pktId;
        e.t0     = simTime();
        e.representative = ul;              // zachowujemy ten egzemplarz
        e.receptions.push_back({inGateIdx, rssi});

        // self-message kończący okno dedup
        e.timer = new cMessage("dedupTimer", KIND_DEDUP_TIMER);
        e.timer->addPar("devIdx").setLongValue(devIdx);
        e.timer->addPar("pktId").setLongValue(pktId);
        scheduleAt(simTime() + dedupHoldoff, e.timer);

        dedup.emplace(K, std::move(e));
    } else {
        // Kolejna kopia w oknie — tylko zapisujemy i zwalniamy
        it->second.receptions.push_back({inGateIdx, rssi});
        delete ul;
    }
}

void LNS::onDedupTimer(cMessage* self)
{
    long devIdx = self->par("devIdx").longValue();
    long pktId  = self->par("pktId").longValue();
    long long K = makeKey(devIdx, pktId);

    auto it = dedup.find(K);
    if (it == dedup.end()) { delete self; return; }

    // Zabierz wpis i skasuj timer
    DedupEntry e = std::move(it->second);
    dedup.erase(it);
    delete self;

    // Wybierz najlepsze wyjście wg ŚREDNIEGO RSSI dla tego urządzenia
    int bestGate = -1;
    double bestMean = -std::numeric_limits<double>::infinity();
    auto &perGate = avgRssiByDevGate[devIdx];

    // Rozważamy bramki, które faktycznie widziały bieżący UL
    for (const auto& r : e.receptions) {
        auto itg = perGate.find(r.gateIdx);
        double mean = (itg != perGate.end()) ? itg->second.mean() : r.rssi; // fallback
        if (mean > bestMean) {
            bestMean = mean;
            bestGate = r.gateIdx;
        }
    }
    if (bestGate < 0) bestGate = 0;

    // Przetwórz JEDEN, zdeduplikowany uplink (wygeneruj ewentualny DL)
    processUniqueUplink(e, bestGate);
}

void LNS::processUniqueUplink(DedupEntry& e, int preferOutGate)
{
    LoRaPacket* ul = e.representative;

    // Pojedynczy wpis do Keepera (tak jak wcześniej, ale TYLKO raz)
    keeper->recordMessageReception(ul->par("KeeperIndex"), ul->par("PacketId"));

    // Oblicz, kiedy powinna być odpowiedź (jak u Ciebie)
    const double respT = responseTime();
    const simtime_t when = e.t0 + respT;

    // Przygotuj odpowiedź na podstawie logiki z Twojego LNS
    LoRaPacket *response = ul->dupLoRa();
    response->gateLNS = preferOutGate; // to wyjście wybierze gałąź self-message

    // JOIN REQUEST (z rozróżnieniem na relay)
    if (ul->getKind() == (short)MsgType::JOINREQUEST)
    {
        if (ul->isRelayed == 1) {
            response->setKind(MsgType::JOINACCEPT);
            response->isRelayed = 1;
            response->setName("RELAY JOIN ACCEPT");
            response->length = 14;
            scheduleAt(when, response);
        } else {
            response->setKind(MsgType::JOINACCEPT);
            response->setName("JOIN ACCEPT");
            scheduleAt(when, response);
        }
        EV << simTime() << " LNS Scheduling join accept in " << (when - simTime()) << "s via json[" << preferOutGate << "]\n";
    }
    // Zwykły UPLINK wymagający ACK
    else if (ul->getKind() == (short)MsgType::UPLINK)
    {
        if (response->ackRequired)
        {
            response->setKind(MsgType::ACK);
            response->setName("ACK");

            response->relayRequired = ul->isRelayed;
            response->isRelayed = 0;
            response->length = 15;
            if (uniform(0,100) > 95)
                response->length = 50;

            scheduleAt(when, response);
            EV << simTime() << " LNS Scheduling ACK in " << (when - simTime()) << "s via json[" << preferOutGate << "]\n";
        }
        else {
            delete response; // brak odpowiedzi wymaganej => nic nie wysyłamy
        }
    }
    else {
        // DOWNLINK lub inne — LNS nie odpowiada
        delete response;
    }

    // Sprzątanie oryginału UL
    delete ul;
}


} //namespace
