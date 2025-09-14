package opp_lora_relay_official.simulations;

import ned.IUnidirectionalChannel;
import opp_lora_relay_official.Endnode;
import opp_lora_relay_official.IpBridge;
import opp_lora_relay_official.Relay;
import opp_lora_relay_official.Gateway;
import opp_lora_relay_official.Keeper;
import opp_lora_relay_official.LNS;

//
// Daily-read scenario with a single Gateway.
// - Endnodes are randomly placed over the canvas.
// - The number of endnodes is configured via the 'numEndnodes' ini parameter.
// - Endnode icons are smaller (configurable via endnodeIconSize).
//
// NOTE:
//  - We ASSIGN to endnode.xpos/ypos (these must exist in Endnode NED type).
//  - We do NOT declare new parameters inside submodules (NED forbids that).
//
network DailyRead_1GW
{
    parameters:
        int numEndnodes = default(500);
        double playgroundWidth  = default(1400);
        double playgroundHeight = default(900);
        int endnodeIconSize = default(10);
        int framesPerWindowMin = default(1);
        int framesPerWindowMax = default(1);
        double gw1x = default(playgroundWidth*0.33);
        double gw1y = default(playgroundHeight/2);
        int transmittingWindowDuration1 = default(180);
        @display("b=$playgroundWidth,$playgroundHeight");



    submodules:
        keeper: Keeper {
            @display("p=136.86751,452.0775;r=20,#FFFF80,#FF8080,2");
        }
        endnode[numEndnodes]: Endnode {
            parameters:
                xpos = uniform(20, parent.playgroundWidth-20);
                ypos = uniform(20, parent.playgroundHeight-20);
            @display("p=$xpos,$ypos;i=device/wifilaptop;is=$endnodeIconSize,$endnodeIconSize");
        }
        gateway: Gateway {
            xpos = parent.gw1x;
            ypos = parent.gw1y;
            @display("p=$gw1x,$gw1y;i=device/antennatower;is=26,26");
        }
        lns: LNS {
            @display("p=80,40;i=abstract/db;is=18,18");
        }


    connections:
        gateway.json <--> lns.json++;
}
