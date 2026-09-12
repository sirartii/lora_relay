#include "LogicGateway.h"
#include "LoRaPacket.h"

namespace opp_lora_relay_official {

Define_Module(LogicGateway);

void LogicGateway::initialize() {}

void LogicGateway::handleMessage(omnetpp::cMessage *msg)
{
    auto *packet = dynamic_cast<LoRaPacket *>(msg);
    if (packet == nullptr) {
        delete msg;
        return;
    }
    if (msg->arrivedOn("logicIn") && packet->autodetectUplink()) {
        send(packet, "json$o");
        return;
    }
    if (msg->arrivedOn("json$i")) {
        send(packet, "logicOut");
        return;
    }
    delete msg;
}

} // namespace opp_lora_relay_official
