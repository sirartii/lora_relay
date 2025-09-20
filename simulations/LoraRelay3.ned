//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

package opp_lora_relay_official.simulations;

import ned.IUnidirectionalChannel;
import opp_lora_relay_official.Endnode;
import opp_lora_relay_official.IpBridge;
import opp_lora_relay_official.Relay;
import opp_lora_relay_official.Gateway;
import opp_lora_relay_official.Keeper;
import opp_lora_relay_official.LNS;

//
// Sample network, consisting of a source and a sink.
//
network LoraRelay3
{
    parameters:
        int numEndnodes = default(15);
        double playgroundWidth = default(1400); // pixels (canvas)
        double playgroundHeight = default(900);
        double gw1x = 750; //default(playgroundWidth/2);
        double gw1y = 450;  //default(playgroundHeight/2);
		int framesPerWindowMin = default(1);
		int framesPerWindowMax = default(1);
		int transmittingWindowDuration1 = default(180);
        double R  = 350;

        @display("bgb=1000,900");
    types:
    submodules:
        keeper: Keeper {
            @display("p=136.86751,452.0775;r=20,#FFFF80,#FF8080,2");
        }

        endnode[numEndnodes]: Endnode {
            parameters:

                xpos   = parent.gw1x + parent.R * sqrt(uniform(0,1)) * cos(uniform(0, 2*3.14159));
                ypos   = parent.gw1y + parent.R * sqrt(uniform(0,1)) * sin(uniform(0, 2*3.14159));

                //xpos = uniform(parent.gw1x - 200, parent.gw1x + 200);
                //ypos = uniform(parent.gw1y -200, parent.gw1y + 200);
                @display("p=$xpos,$ypos;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

        endnode001: Endnode {
            parameters:
                xpos = 108+uniform(1,300);
                ypos = 250+uniform(1,300);
                @display("p=$xpos,$ypos;;r=1,#0080C0,#FF0080,2;t=test decoration");
        }
        endnode002: Endnode {
            parameters:
                xpos = 108+uniform(1,300);
                ypos = 200+uniform(1,300);
                @display("p=$xpos,$ypos;;r=1,#0080C0,#FF0080,2;t=test decoration");
        }
        endnode003: Endnode {
            parameters:
                xpos = 108+uniform(1,300);
                ypos = 150+uniform(1,300);
                @display("p=$xpos,$ypos;;r=1,#0080C0,#FF0080,2;t=test decoration");
        }



        endnode004: Endnode {
            parameters:
                xpos = 75+uniform(1,300);
                ypos = parent.playgroundHeight-50-uniform(1,300);
                @display("p=$xpos,$ypos;;r=1,#0080C0,#FF0080,2;t=test decoration");
        }
        endnode005: Endnode {
            parameters:
                xpos = 75+uniform(1,300);
                ypos = parent.playgroundHeight-75-uniform(1,300);
                @display("p=$xpos,$ypos;;r=1,#0080C0,#FF0080,2;t=test decoration");
        }



//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        gateway: Gateway {
            parameters:
                xpos = parent.gw1x;
                ypos = parent.gw1y;
                @display("p=$xpos,$ypos;;r=1,#FF8000,black,2");
        }

        relay01: Relay {
            parameters:
                xpos = 300;
                ypos = 250;
                @display("p=$xpos,$ypos;;r=1,#FF8000,black,2");
        }

        gateway3: Relay {
            parameters:
                xpos = 300;
                ypos = 650;
                @display("p=$xpos,$ypos;;r=1,#FF8000,black,2");
        }


        //relay01: Relay {
        //    parameters:
        //        xpos = 608;
        //        ypos = 261;
        //        @display("p=$xpos,$ypos;;r=1,#0080C0,#FF0080,2;t=test decoration");
        //}

        lns01: LNS {
            @display("p=53,91");
        }

        //ipb1: IpBridge {
        //    parameters:
        //        xpos = 1123;
        //        ypos = 108;
        //        @display("p=$xpos,$ypos;");
        //}
    connections:
        gateway.json <--> lns01.json++;
        //ipb1.json <--> lns01.json++;
}
