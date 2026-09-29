import opp_lora_relay_official.GatewayRadio;
import opp_lora_relay_official.Keeper;

simple MultichannelRadioTest extends GatewayRadio {
    parameters:
        @class(MultichannelRadioTest);
        int testScenario;
}
simple MultichannelTestSink {
    parameters: @class(MultichannelTestSink);
    gates: input in;
}
module MultichannelTestHost {
    parameters:
        double xpos = default(100);
        double ypos = default(100);
    gates: input radioIn @directIn;
    submodules:
        radio: MultichannelRadioTest;
        sink: MultichannelTestSink;
    connections allowunconnected:
        radio.radioIn <-- radioIn;
        radio.toLogic --> sink.in;
}
network MultichannelTest {
    parameters:
        int transmittingWindowDuration1 = default(720);
        int framesPerWindowMin = default(1);
    submodules:
        keeper: Keeper;
        gateway: MultichannelTestHost;
}
