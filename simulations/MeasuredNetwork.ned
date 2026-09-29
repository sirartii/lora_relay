package opp_lora_relay_official.simulations;

import opp_lora_relay_official.Endnode;
import opp_lora_relay_official.Gateway;
import opp_lora_relay_official.Keeper;
import opp_lora_relay_official.LNS;

// Deterministic measured coordinates supplied by the generated .ini.
// Deliberately no substitute gateway position. Missing coordinates must fail.
network MeasuredNetwork {
    parameters:
        int numEndnodes;
        int transmittingWindowDuration1 = default(720);
        int transmittingWindowDuration2 = default(720);
        int framesPerWindowMin = default(1);
        int framesPerWindowMax = default(1);
        double gwX;
        double gwY;
    submodules:
        keeper: Keeper;
        endnode[numEndnodes]: Endnode;
        gateway[1]: Gateway {
            parameters:
                xpos = parent.gwX;
                ypos = parent.gwY;
        }
        lns01: LNS;
    connections:
        gateway[0].json <--> lns01.json++;
}
