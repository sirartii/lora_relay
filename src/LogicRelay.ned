package opp_lora_relay_official;

simple LogicRelay
{
    parameters:
        int retransmissionModel;
        double ts011WorAckDelay @unit(s) = default(50ms);
        double ts011WorDataDelay @unit(s) = default(50ms);
        double ts011UplinkListenTimeout @unit(s) = default(8s);
        double ts011RelayForwardDelay @unit(s) = default(50ms);
        double ts011RxrDelay @unit(s) = default(18s);
        double epRxrDelay @unit(s) = default(7s);
        double ts011WorAckProbability = default(1);
        double ts011CadPeriodicity @unit(s) = default(1s);
        int ts011CadToRxSymbols = default(8);
        int ts011RelayClockAccuracyPpm = default(40);
        int ts011RelaySpreadingFactor = default(9);
        int ts011RelayUplinkChannel = default(1);
        int ts011RxrChannel = default(1);
        int ts011MaximumForwardedPayload = default(222);
        bool ts011ForwardingEnabled = default(true);
        string ts011JoinFilterRules = default("forward:0:"); // action:lengthBytes:over-air hex, max 16 rules

        // A negative reload rate means "no limit". Buckets start with one
        // hour's reload-rate worth of tokens, as required by TS011.
        double ts011JoinReloadRate = default(4);
        double ts011JoinBucketSize = default(8);
        double ts011PerDeviceReloadRate = default(-1);
        double ts011PerDeviceBucketSize = default(0);
        double ts011TrustedReloadRate = default(8);
        double ts011TrustedBucketSize = default(16);
        double ts011GlobalReloadRate = default(8);
        double ts011GlobalBucketSize = default(16);
    gates:
        output logicOut;
        input logicIn;
}
