package opp_lora_relay_official;

// Shared physical-layer parameters. Concrete radio roles below keep gateway,
// Class A end-device, relay and IP-bridge state machines separate in C++.
simple Radio
{
    parameters:
        @display("i=block/wrxtx");
        double txPower = default(14);
        int forceSF = default(12);
        double pathlosStd;
        double sensitivityAllRadios;
        double relayLinkMarginDb = default(10);
        double ipBridgeLinkMarginDb = default(50);

        int listeningWindowStart1;
        int listeningWindowStart2;
        int listeningWindowDuration1;
        int listeningWindowDuration2;
        double listeningWindowRX1;
        double listeningWindowRX2;
        double listeningWindowRXR;
        double listeningWindowRXC;
        double listeningWindowRXCR;

        int ts011DefaultWorChannel = default(1);
        int ts011SecondWorChannel = default(2);
        bool ts011SecondChannelEnabled = default(false);
        int ts011Rx2Channel = default(1);
        int ts011RxrChannel = default(1);
        double ts011CadPeriodicity @unit(s) = default(1s);
        int ts011CadToRxSymbols = default(8);
        int ts011ChannelScanSymbols = default(8);
        double ts011WorAckDelay @unit(s) = default(1s);
        double ts011WorDataDelay @unit(s) = default(1s);
        double ts011WorAckWindow @unit(s) = default(1s);
        double ts011UplinkListenTimeout @unit(s) = default(8s);

        double currentConsumptionRx;
        double currentConsumptionTx;
        double currentConsumptionIdle;
        double currentConsumptionWOR;
        double currentConsumptionBaseOffset = default(0);
        double lteAttachEnergyMAs = default(0);
        double lteTransmitEnergyMAs = default(0);
        double clockDriftMax;
        int retransmissionModel;
        @signal[wakeupRelay](type=double);
    gates:
        input radioIn;
        output toLogic;
        input fromLogic;
}

simple GatewayRadio extends Radio { @class(GatewayRadio); }
simple EndDeviceRadio extends Radio { @class(EndDeviceRadio); }
simple RelayRadio extends Radio { @class(RelayRadio); }
simple IpBridgeRadio extends Radio { @class(IpBridgeRadio); }
