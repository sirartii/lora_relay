#ifndef OPP_LORA_RELAY_LOGIC_RELAY_H
#define OPP_LORA_RELAY_LOGIC_RELAY_H

#include <omnetpp.h>

#include <unordered_map>

#include "common.h"

namespace opp_lora_relay_official {

class LoRaPacket;

class LogicRelay : public omnetpp::cSimpleModule
{
  protected:
    void initialize() override;
    void handleMessage(omnetpp::cMessage *msg) override;
    void finish() override;

  private:
    struct TokenBucket {
        double tokens = 0;
        double reloadRate = -1;
        double capacity = 0;
        omnetpp::simtime_t lastReload;

        void configure(double rate, double size, omnetpp::simtime_t now);
        void reload(omnetpp::simtime_t now);
        bool available(omnetpp::simtime_t now);
        bool consume(omnetpp::simtime_t now);
        double secondsUntilToken(omnetpp::simtime_t now);
    };

    struct WorContext {
        int channel = 1;
        int uplinkChannel = 1;
        unsigned int counter = 0;
        omnetpp::simtime_t receivedAt;
    };

    bool canForward(int deviceId, bool joinRequest, bool consume);
    int forwardingStatus(int deviceId);
    TokenBucket &deviceBucket(int deviceId);
    void handleWor(LoRaPacket *packet);
    void handleAccessUplink(LoRaPacket *packet);
    void handleForwardDownlink(LoRaPacket *packet);

    RetransmissionModel model = RetransmissionModel::UNKNOWN;
    double worAckDelay = 1;
    double relayForwardDelay = 1;
    double rxrDelay = 7;
    double worAckProbability = 1;
    int cadPeriodicityMs = 1000;
    int cadToRxSymbols = 8;
    int relayClockAccuracyPpm = 40;
    int relaySpreadingFactor = 9;
    int relayUplinkChannel = 3;
    int rxrChannel = 1;
    int maximumForwardedPayload = 222;
    bool forwardingEnabled = true;

    double perDeviceRate = -1;
    double perDeviceCapacity = 0;
    TokenBucket joinBucket;
    TokenBucket aggregateTrustedBucket;
    TokenBucket globalBucket;
    std::unordered_map<int, TokenBucket> perDeviceBuckets;
    std::unordered_map<int, WorContext> worContexts;

    long worReceived = 0;
    long worAcknowledged = 0;
    long uplinksForwarded = 0;
    long downlinksForwarded = 0;
    long forwardingLimitDrops = 0;
    long payloadLimitDrops = 0;
};

} // namespace opp_lora_relay_official

#endif
