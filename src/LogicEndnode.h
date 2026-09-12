#ifndef OPP_LORA_RELAY_LOGIC_ENDNODE_H
#define OPP_LORA_RELAY_LOGIC_ENDNODE_H

#include <omnetpp.h>

#include "Keeper.h"
#include "common.h"

namespace opp_lora_relay_official {

class LoRaPacket;

class LogicEndnode : public omnetpp::cSimpleModule
{
  public:
    LogicEndnode() = default;
    ~LogicEndnode() override;
    int myKeeperIndex = -1;

  protected:
    void initialize() override;
    void handleMessage(omnetpp::cMessage *msg) override;
    void finish() override;

  private:
    void scheduleInitialWindow(omnetpp::cMessage *timer, int windowIndex);
    void scheduleFollowingWindow(omnetpp::cMessage *timer, int windowIndex);
    void beginApplicationTransmission();
    void scheduleTs011Wor(int packetId, bool join, bool ackRequired);
    void sendTs011Wor(omnetpp::cMessage *event);
    void sendDataFrame(int packetId, bool join, bool ackRequired, bool relayed);
    void startResponseTimeout();
    void handleResponseTimeout(omnetpp::cMessage *msg);
    void handleDownlink(LoRaPacket *packet);
    void handleWorAck(LoRaPacket *packet);
    void updateLogicalState(EndDeviceState state);
    void updateTs011MissedAck(unsigned int counter);
    void recordOnline();
    int chooseWorChannel() const;
    int unsynchronizedPreambleSymbols(int spreadingFactor) const;
    omnetpp::simtime_t synchronizedWorStart(int spreadingFactor, int channel,
                                           int &preambleSymbols);

    Keeper *keeper = nullptr;
    EndDeviceState endDeviceState = EndDeviceState::OFFLINE;
    RetransmissionModel model = RetransmissionModel::UNKNOWN;
    Ts011SyncState syncState = Ts011SyncState::INITIALIZED;

    int transmittingWindowStart[2]{0, 720};
    int transmittingWindowDuration[2]{180, 180};
    int windowsPerDay = 2;
    int framesPerWindowMin = 1;
    int framesPerWindowMax = 1;
    int framesRemaining[2]{1, 1};
    omnetpp::cMessage *windowTimers[2]{nullptr, nullptr};
    omnetpp::cMessage *responseTimer = nullptr;

    int packetCounter = 0;
    int lastAckRequestCount = 0;
    int missedResponse = 0;
    bool waitingResponse = false;
    int applicationPayloadLength = 51;
    int missedPacketsToSearchRelay = 2;
    int forceSpreadingFactor = 12;
    omnetpp::simtime_t lastAckTime;
    omnetpp::simtime_t lastOnline;
    double offlineMax = 0;
    double offlineTotal = 0;
    omnetpp::simsignal_t wakeupRelay;

    int ts011EndDeviceSpreadingFactor = 10;
    int ts011DefaultWorChannel = 1;
    int ts011SecondWorChannel = 2;
    int ts011UplinkChannel = 1;
    bool ts011SecondChannelConfigured = false;
    double ts011CadPeriodicity = 1;
    double knownCadPeriodicity = 1;
    int ts011CadToRxSymbols = 8;
    double ts011WorAckDelay = 1;
    double ts011WorDataDelay = 1;
    double ts011WorValidityProbability = 1;
    int ts011RelayClockAccuracyPpm = 40;
    int ts011EndDeviceClockAccuracyPpm = 40;
    unsigned int worFrameCounter = 0;
    unsigned int lastAcknowledgedWorCounter = 0;
    unsigned int pendingWorCounter = 0;
    bool pendingWorAcknowledged = false;
    int consecutiveWorWithoutAck = 0;
    int lastWorPreambleSymbols = 0;
    int lastWorChannel = 1;
    omnetpp::simtime_t tLast;
    omnetpp::simtime_t tRef;

    long worTransmitted = 0;
    long worAckReceived = 0;
    long syncLosses = 0;
    long invalidWorTransmitted = 0;
    double totalWorPreambleSymbols = 0;
    double totalWorAirtime = 0;
};

} // namespace opp_lora_relay_official

#endif
