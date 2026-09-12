/*
 * LogicIpBridge.cc
 *
 *  Created on: 6 wrz 2025
 *      Author: User
 */

// LogicIpBridge.cc
#include "LogicIpBridge.h"
#include "common.h"
#include "LoRaPacket.h"

using namespace omnetpp;

namespace opp_lora_relay_official {

Define_Module(LogicIpBridge);

void LogicIpBridge::initialize()
{
    // parametr na przyszłość (opcjonalne wsparcie DL)
    if (hasPar("forwardDownlink"))
        forwardDownlink = par("forwardDownlink").boolValue();
}

void LogicIpBridge::handleMessage(cMessage *msg)
{
    // Uplink from the radio.
    if (msg->arrivedOn("logicIn")) {
        auto *lp = dynamic_cast<LoRaPacket*>(msg);
        if (!lp) {
            EV << simTime() << " [IpBridge] Non-LoRa on logicIn, dropping.\n";
            delete msg;
            return;
        }

        // Zgodnie z dotychczasową konwencją (jak w LogicGateway)
        // przesyłamy UPLINK do LNS przez idealne IP (bramka json)
        // NOTE: używamy dupLoRa() jak w Twoim kodzie Gateway/LNS
        if (lp->autodetectUplink()) {
            send(lp->dupLoRa(), "json$o");
        } else {
            EV << simTime() << " [IpBridge] Non-uplink LoRa on logicIn (ignored).\n";
        }

        // Sprzątanie oryginału
        delete msg;
        return;
    }

    // (Opcjonalny) DL z LNS -> do radia
    if (msg->arrivedOn("json$i")) {
        if (forwardDownlink) {
            auto *lp = dynamic_cast<LoRaPacket*>(msg);
            if (lp) {
                send(lp->dupLoRa(), "logicOut"); // dalej do Radio.fromLogic
            } else {
                EV << simTime() << " [IpBridge] Non-LoRa on json$i, ignoring.\n";
            }
        } else {
            EV << simTime() << " [IpBridge] Downlink disabled, dropping json$i.\n";
        }
        delete msg;
        return;
    }

    // Gdyby coś trafiło na nieznaną bramkę
    EV << simTime() << " [IpBridge] Message on unknown gate, dropping.\n";
    delete msg;
}

} // namespace


