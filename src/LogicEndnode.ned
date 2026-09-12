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

        int ts011EndDeviceSpreadingFactor = default(10);
        int ts011DefaultWorChannel = default(1);
        int ts011SecondWorChannel = default(2);
        int ts011UplinkChannel = default(1);
        bool ts011SecondChannelEnabled = default(false);
        double ts011CadPeriodicity @unit(s) = default(1s);
        int ts011CadToRxSymbols = default(8);
        double ts011WorAckDelay @unit(s) = default(1s);
        double ts011WorDataDelay @unit(s) = default(1s);
        double ts011WorValidityProbability = default(1);
        int ts011RelayClockAccuracyPpm = default(40);
        int ts011EndDeviceClockAccuracyPpm = default(40);
        @signal[wakeupRelay](type=double);
    gates:
        output logicOut;
        input logicIn;
}
