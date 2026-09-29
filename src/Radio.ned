package opp_lora_relay_official;

// Shared physical-layer parameters. Concrete radio roles below keep gateway,
// Class A end-device, relay and IP-bridge state machines separate in C++.
simple Radio
{
    parameters:
        @display("i=block/wrxtx");
        double txPower = default(14);
        int forceSF = default(12);
        // Opt-in: legacy scenarios keep the original one-channel GW model.
        bool lorawanMultichannel = default(false);
        bool exportGatewayReceives = default(false);
        double pathlosStd;
        double sensitivityAllRadios;
        // ED<->RL access link uses the baseline propagation model by default.
        double relayAccessLinkMarginDb = default(0);
        // RL<->GW backhaul is modeled as LOS with a separate link-budget gain.
        double relayBackhaulLosGainDb = default(10);
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
        double receiveDelay1 @unit(s) = default(1s);
        double joinAcceptDelay1 @unit(s) = default(5s);
        double ts011RxrDelay @unit(s) = default(18s);
        double epRxrDelay @unit(s) = default(7s);

        // Logical non-overlapping frequencies, NOT regional-plan channel indices.
        // Ordinary traffic: legacy ID 1, multichannel IDs 1..8.
        // WOR uses separate IDs 10 and 11 in both modes.
        int ts011DefaultWorChannel = default(10);
        int ts011SecondWorChannel = default(11);
        bool ts011SecondChannelEnabled = default(false);
        int ts011Rx2Channel = default(1);
        int ts011RxrChannel = default(1);
        double ts011CadPeriodicity @unit(s) = default(1s);
        // Dynamic energy is averaged only over time actually spent in WOR.
        // The PHY currently uses BW125 kHz for every LoRa packet.
        bool ts011DynamicWorEnergy = default(true);
        int ts011EndDeviceSpreadingFactor = default(9);
        int ts011CadSymbolNum = default(4);
        double currentConsumptionCad = default(3.84); // mA, reference SX126x
        int ts011CadToRxSymbols = default(8);
        int ts011ChannelScanSymbols = default(8);
        double ts011WorAckDelay @unit(s) = default(50ms);
        double ts011WorDataDelay @unit(s) = default(50ms);
        double ts011WorAckWindow @unit(s) = default(1s);
        double ts011UplinkListenTimeout @unit(s) = default(8s);

        double currentConsumptionRx;
        double currentConsumptionTx;
        double currentConsumptionIdle;
        double currentConsumptionWOR; // legacy, ignored by dynamic TS011 relay
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
