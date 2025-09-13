package opp_lora_relay_official.simulations;

import ned.IUnidirectionalChannel;
import opp_lora_relay_official.Endnode;
import opp_lora_relay_official.IpBridge;
import opp_lora_relay_official.Relay;
import opp_lora_relay_official.Gateway;
import opp_lora_relay_official.Keeper;
import opp_lora_relay_official.LNS;
//
// Daily-read scenario with two Gateways.
// - Endnodes are randomly placed over the canvas.
// - The number of endnodes is configured via the 'numEndnodes' ini parameter.
// - Endnode icons are smaller (configurable via endnodeIconSize).
//
// NOTE:
//  - Gateway positions are defined as network parameters (gwAx, gwAy, gwBx, gwBy)
//    and referenced from the submodules' @display.
//  - Endnode.xpos/ypos are ASSIGNED (must exist in Endnode NED type).
//
network DailyRead_1GW1RL
{
    parameters:
        int numEndnodes = default(50);          // set in omnetpp.ini
        double playgroundWidth  = default(1400); // pixels (canvas)
        double playgroundHeight = default(900);
        int endnodeIconSize = default(10);       // px
        double gwAx = default(playgroundWidth*0.33);
        double gwAy = default(playgroundHeight/2);
        double gwBx = default(playgroundWidth*0.67);
        double gwBy = default(playgroundHeight/2);
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
        gatewayA: Gateway {
            parameters:
                xpos = parent.gwAx;
                ypos = parent.gwAy;
            @display("p=$gwAx,$gwAy;i=device/antennatower;is=26,26");
        }
        relay: Relay {
            parameters:
                xpos = parent.gwBx;
                ypos = parent.gwBy;
            @display("p=$gwBx,$gwBy;i=device/antennatower;is=26,26");
        }
        lns: LNS {
            @display("p=80,40;i=abstract/db;is=18,18");
        }

    connections:
        gatewayA.json <--> lns.json++;
}
