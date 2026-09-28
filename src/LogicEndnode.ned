package opp_lora_relay_official;

simple LogicEndnode
{
    parameters:
        volatile double sendInterval @unit(s) = default(exponential(60min)); // legacy, window scheduler is authoritative
        int transmittingWindowStart1;
        int transmittingWindowStart2;
        int transmittingWindowDuration1;
        int transmittingWindowDuration2;
        int missedPacketsToSearchRelay;
        int retransmissionModel;
        double clockDriftMax;
        int framesPerWindowMin = default(1);
        int framesPerWindowMax = default(framesPerWindowMin);
        int windowsPerDay = default(2);
        int forceSF = default(12);
        int applicationPayloadLength = default(51);
        int ackEvery = default(5);
        int ts011MissedAcksToRelay = default(1);
        int ts011JoinRelayEvery = default(4);
        double receiveDelay1 @unit(s) = default(1s);
        double joinAcceptDelay1 @unit(s) = default(5s);
        double ts011RxrDelay @unit(s) = default(18s);
        double epRxrDelay @unit(s) = default(7s);
        double receiveWindowDuration @unit(s) = default(1s);

        int ts011EndDeviceSpreadingFactor = default(10);
        int ts011DefaultWorChannel = default(10);
        int ts011SecondWorChannel = default(11);
        int ts011UplinkChannel = default(1);
        bool ts011SecondChannelEnabled = default(false);
        double ts011CadPeriodicity @unit(s) = default(1s);
        int ts011CadToRxSymbols = default(8);
        double ts011WorAckDelay @unit(s) = default(50ms);
        double ts011WorDataDelay @unit(s) = default(50ms);
        double ts011WorValidityProbability = default(1);
        int ts011RelayClockAccuracyPpm = default(40);
        int ts011EndDeviceClockAccuracyPpm = default(40);
        @signal[wakeupRelay](type=double);
    gates:
        output logicOut;
        input logicIn;
}
