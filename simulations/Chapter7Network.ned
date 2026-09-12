package opp_lora_relay_official.simulations;

import opp_lora_relay_official.Endnode;
import opp_lora_relay_official.Gateway;
import opp_lora_relay_official.IpBridge;
import opp_lora_relay_official.Keeper;
import opp_lora_relay_official.LNS;
import opp_lora_relay_official.Relay;

// Parametric topology for reproducing the scenarios and plots from chapter 7.
// The central population and both weak islands follow section 7.1.
network Chapter7Network
{
    parameters:
        int numEndnodes = default(50);
        int extranodex1 = int((numEndnodes * 3) / 50);
        int extranodex2 = int((numEndnodes * 2) / 50);
        int numGateways = default(1);
        int numRelays = default(0);
        int numIpBridges = default(0);

        double playgroundWidth = default(1400);
        double playgroundHeight = default(900);
        double gw1x = default(750);
        double gw1y = default(450);
        double gw2x = default(150);
        double gw2y = default(250);
        double gw3x = default(250);
        double gw3y = default(750);
        double R = default(350);

        int framesPerWindowMin = default(1);
        int framesPerWindowMax = default(1);
        int transmittingWindowDuration1 = default(720);
        int transmittingWindowDuration2 = default(720);

        @display("bgb=1400,900");

    submodules:
        keeper: Keeper {
            @display("p=136,452;r=20,#FFFF80,#FF8080,2");
        }

        endnode[numEndnodes]: Endnode {
            parameters:
                xpos = parent.gw1x + parent.R * sqrt(uniform(0,1)) * cos(uniform(0, 2*3.14159));
                ypos = parent.gw1y + parent.R * sqrt(uniform(0,1)) * sin(uniform(0, 2*3.14159));
            @display("p=$xpos,$ypos;r=1,#FF80FF,#0080FF,2");
        }

        endnode1[extranodex1]: Endnode {
            parameters:
                xpos = 108 + uniform(1,300);
                ypos = 250 + uniform(1,300);
            @display("p=$xpos,$ypos;r=1,#0080C0,#FF0080,2");
        }

        endnode2[extranodex2]: Endnode {
            parameters:
                xpos = 50 + uniform(1,300);
                ypos = parent.playgroundHeight - 50 - uniform(1,300);
            @display("p=$xpos,$ypos;r=1,#0080C0,#FF0080,2");
        }

        gateway[numGateways]: Gateway {
            parameters:
                xpos = index == 0 ? parent.gw1x : (index == 1 ? parent.gw2x : parent.gw3x);
                ypos = index == 0 ? parent.gw1y : (index == 1 ? parent.gw2y : parent.gw3y);
            @display("p=$xpos,$ypos;r=1,#FF8000,black,2");
        }

        relay[numRelays]: Relay {
            parameters:
                xpos = 300;
                ypos = index == 0 ? 250 : 650;
            @display("p=$xpos,$ypos;r=1,#00A000,black,2");
        }

        // The lower-case name is intentional: Radio::initialize() uses the
        // module-name prefix to classify this radio as IPBRIDGE.
        ipbridge[numIpBridges]: IpBridge {
            parameters:
                xpos = 300;
                ypos = index == 0 ? 250 : 650;
            @display("p=$xpos,$ypos;r=1,#0080C0,black,2");
        }

        lns01: LNS {
            @display("p=53,91");
        }

    connections:
        for i=0..numGateways-1 {
            gateway[i].json <--> lns01.json++;
        }
        for i=0..numIpBridges-1 {
            ipbridge[i].json <--> lns01.json++;
        }
}
