// Linked only into the test executable, not into the production simulator.
#include "Radio.h"
#include <array>
using namespace omnetpp;
using namespace opp_lora_relay_official;

class MultichannelRadioTest : public GatewayRadio {
  protected:
    void event(double time, int channel, double power = -80, bool tx = false) {
        auto *m = new cMessage(tx ? "test TX" : "test air arrival", 32000);
        m->addPar("channel") = channel;
        m->addPar("power") = power;
        m->addPar("tx") = tx;
        scheduleAt(SimTime(time), m);
    }
    void initialize() override {
        GatewayRadio::initialize();
        const int scenario = par("testScenario").intValue();
        if (scenario == 0) {
            for (int channel = 1; channel <= 8; ++channel) event(1, channel);
        }
        else if (scenario == 1 || scenario == 2) {
            event(1, 1); event(1.1, 1, scenario == 2 ? -70 : -80);
        }
        else if (scenario == 3) {
            event(1, 1); event(1, 2); event(1.2, 8, -80, true);
            event(1.3, 3); event(5, 4);
        }
        else if (scenario == 4) {
            event(0.1, 8, -80, true); event(2.5, 1); event(3.2, 1); event(6.5, 1);
        }
        else if (scenario == 5) {
            LoRaPacket p("duration", MsgType::UPLINK); p.length = 51; p.spreadingFactor = 12;
            event(1, 1); event((SimTime(1) + SimTime(p.calcMsgDuration())).dbl(), 1);
        }
        else if (scenario == 6) {
            event(1, 1, -130); event(1.1, 1, -80);
        }
        else throw cRuntimeError("Unknown test scenario");
    }
    void handleMessage(cMessage *msg) override {
        if (!msg->isSelfMessage() || msg->getKind() != 32000) {
            GatewayRadio::handleMessage(msg); return;
        }
        const bool tx = msg->par("tx").boolValue();
        auto *packet = new LoRaPacket("test packet", tx ? MsgType::ACK : MsgType::UPLINK);
        packet->isUplink = !tx;
        packet->length = 51;
        packet->spreadingFactor = 12;
        packet->channel = msg->par("channel").longValue();
        packet->senderPositionX = position.x;
        packet->senderPositionY = position.y;
        packet->transmissionPower = msg->par("power").doubleValue();
        if (tx) sendToNetwork(packet);
        else receiveFromNetwork(packet);
        delete msg;
    }
    void finish() override {
        GatewayRadio::finish();
        std::array<long, 6> actual{};
        for (int channel = 1; channel <= 8; ++channel)
            for (int result = 1; result <= 5; ++result) actual[result] += gatewayOutcomes[channel][result];
        // pending, delivered, sensitivity, collision, tx_blocked, tx_interrupted
        const std::array<std::array<long, 6>, 7> expected{{
            {{0,8,0,0,0,0}}, {{0,0,0,2,0,0}}, {{0,1,0,1,0,0}},
            {{0,1,0,0,1,2}}, {{0,1,0,1,1,0}}, {{0,2,0,0,0,0}}, {{0,1,1,0,0,0}}
        }};
        if (actual != expected.at(par("testScenario").intValue()) || !gatewayTimers.empty())
            throw cRuntimeError("Multichannel integration assertion failed");
        recordScalar("Multichannel integration passed", 1);
    }
};
Define_Module(MultichannelRadioTest);

class MultichannelTestSink : public cSimpleModule {
    void handleMessage(cMessage *msg) override { delete msg; }
};
Define_Module(MultichannelTestSink);
