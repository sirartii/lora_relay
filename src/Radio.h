#ifndef OPP_LORA_RELAY_RADIO_H
#define OPP_LORA_RELAY_RADIO_H

#include <omnetpp.h>

#include <list>
#include <string>

#include "Keeper.h"
#include "LoRaPacket.h"
#include "common.h"

namespace opp_lora_relay_official {

class Radio : public omnetpp::cSimpleModule, public omnetpp::cListener
{
  public:
    Radio() = default;
    virtual ~Radio();

    bool isListening() const;
    bool isOffOrUnknown() const noexcept;
    bool acceptsAirDelivery(const LoRaPacket *packet) const;
    DevType getDeviceType() const noexcept { return deviceType; }

  protected:
    virtual DevType configuredDeviceType() const = 0;
    virtual void updateIdleRadioState() = 0;
    virtual bool roleAcceptsPacket(const LoRaPacket *packet) const;

    void initialize() override;
    void handleMessage(omnetpp::cMessage *msg) override;
    void receiveSignal(omnetpp::cComponent *src, omnetpp::simsignal_t id,
                       double value, omnetpp::cObject *details) override;
    void finish() override;
    using omnetpp::cListener::finish;

    void updateRadio();
    void sendToNetwork(LoRaPacket *msg);
    void directSendWithDelete(LoRaPacket *msg);
    void receiveFromNetwork(LoRaPacket *msg);
    void dropByCollision(LoRaPacket *packet);
    void updateRadioActivity(RadioActivity::ra activity);
    void updateDaytime();
    void openClassAWindow(int index, omnetpp::cMessage *timer);
    void scheduleClassAReception(bool includeRxr);
    bool relayScanOverlaps(const LoRaPacket *packet, omnetpp::simtime_t *scanTime = nullptr) const;
    double getRssi(LoRaPacket *msg);
    double calculateReceivedPower(float pSend, double distance);

    Keeper *keeper = nullptr;
    int myKeeperIndex = -1;
    DevType deviceType = DevType::DEVUNKNOWN;
    RetransmissionModel retransmissionModel = RetransmissionModel::UNKNOWN;
    RadioActivity::ra radioActivity = RadioActivity::UNKNOWN;
    Coords position{0, 0};
    std::string moduleLabel;

    double sensitivity = -114;
    double txPower = 14;
    int dayTime = 0;
    omnetpp::simtime_t busyUntil;
    omnetpp::simtime_t relayListeningUntil;
    omnetpp::simtime_t relayExpectedUplinkUntil;
    int relayExpectedDataChannel = 1;
    int activeReceiveChannel = 1;
    int lastSpreadingFactor = 12;
    int collisionCount = 0;
    double rssiOfCurrentReceive = -200;
    bool disableClassAWindows = false;
    bool rxrPending = false;
    bool downlinkReceivedInClassA = false;

    int listeningWindowStart1 = 0;
    int listeningWindowStart2 = 0;
    int listeningWindowDuration1 = 0;
    int listeningWindowDuration2 = 0;
    double listeningWindowRX1 = 1;
    double listeningWindowRX2 = 1;
    double listeningWindowRXR = 1;
    double listeningWindowRXC = 1;
    double listeningWindowRXCR = 3;

    int ts011DefaultWorChannel = 1;
    int ts011SecondWorChannel = 2;
    bool ts011SecondChannelEnabled = false;
    int ts011Rx2Channel = 1;
    int ts011RxrChannel = 1;
    double ts011CadPeriodicity = 1;
    int ts011CadToRxSymbols = 8;
    int ts011ChannelScanSymbols = 8;
    double ts011WorAckDelay = 1;
    double ts011WorDataDelay = 1;
    double ts011WorAckWindow = 1;
    double ts011UplinkListenTimeout = 8;

    LoRaPacket *currentRcvMessage = nullptr;
    omnetpp::cMessage *radioRXTimer = nullptr;
    omnetpp::cMessage *radioUpdateReceive = nullptr;
    omnetpp::cMessage *timerListeningWindowStart1 = nullptr;
    omnetpp::cMessage *timerListeningWindowStop1 = nullptr;
    omnetpp::cMessage *timerListeningWindowStart2 = nullptr;
    omnetpp::cMessage *timerListeningWindowStop2 = nullptr;
    std::list<LoRaPacket> sendQueue;

    double currentConsumptionRx = 0;
    double currentConsumptionTx = 0;
    double currentConsumptionIdle = 0;
    double currentConsumptionWOR = 0;
    double currentConsumptionBaseOffset = 0;
    double lteAttachEnergyMAs = 0;
    double lteTransmitEnergyMAs = 0;
    double energyConsumptionRx = 0;
    double energyConsumptionTx = 0;
    double energyConsumptionIdle = 0;
    double energyConsumptionWOR = 0;
    double energyConsumptionBaseOffset = 0;
    double energyConsumptionLteAttach = 0;
    double energyConsumptionLteTransmit = 0;
    long lteAttachCount = 0;
    long lteTransmitCount = 0;
    omnetpp::simtime_t lastRadioActivityChange;
    omnetpp::simsignal_t wakeupRelay;
};

class GatewayRadio : public Radio
{
  protected:
    DevType configuredDeviceType() const override { return DevType::GATEWAY; }
    void updateIdleRadioState() override;
    bool roleAcceptsPacket(const LoRaPacket *packet) const override;
};

class EndDeviceRadio : public Radio
{
  protected:
    DevType configuredDeviceType() const override { return DevType::ENDNODE; }
    void updateIdleRadioState() override;
    bool roleAcceptsPacket(const LoRaPacket *packet) const override;
};

class RelayRadio : public Radio
{
  protected:
    DevType configuredDeviceType() const override { return DevType::RELAY; }
    void updateIdleRadioState() override;
    bool roleAcceptsPacket(const LoRaPacket *packet) const override;
};

class IpBridgeRadio : public Radio
{
  protected:
    DevType configuredDeviceType() const override { return DevType::IPBRIDGE; }
    void updateIdleRadioState() override;
    bool roleAcceptsPacket(const LoRaPacket *packet) const override;
};

} // namespace opp_lora_relay_official

#endif
