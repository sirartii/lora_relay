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
network LoraRelay2Gateways
{
    parameters:
        int numEndnodes = default(15);

        @display("bgb=1685.2676,526.73254");
    types:
    submodules:
        keeper: Keeper {
            @display("p=136.86751,452.0775;r=20,#FFFF80,#FF8080,2");
        }

        endnode: Endnode {
            @display("p=266,435,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        gateway1: Gateway {

            @display("p=279,49;r=1,#FF8000,black,2");
        }
//        endnode1[1]: Endnode {
//            @display("p=619.36005,345.625");
//        }

        lns01: LNS {
            @display("p=785,49");
        }
//
//        relay02: Relay {
//        }

        gateway2: Gateway {
            @display("p=1294,49;r=1000,#80FFFF,#400000,2");
        }
        endnode1: Endnode {
            @display("p=266,323,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode2: Endnode {
            @display("p=262,380,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode3: Endnode {
            @display("p=262,211,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode4: Endnode {
            @display("p=150,80;r=1000,#FF80FF,#0080FF,2");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode5: Endnode {
            @display("p=264,177,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode6: Endnode {
            @display("p=376,196,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode7: Endnode {
            @display("p=378,422,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode8: Endnode {
            @display("p=378,163,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode9: Endnode {
            @display("p=378,308,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode10: Endnode {
            @display("p=376,365,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode11: Endnode {
            @display("p=152,209,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode12: Endnode {
            @display("p=154,435,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode13: Endnode {
            @display("p=154,175,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode14: Endnode {
            @display("p=154,321,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode15: Endnode {
            @display("p=152,378,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode16: Endnode {
            @display("p=486,196,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode17: Endnode {
            @display("p=488,422,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode18: Endnode {
            @display("p=488,163,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode19: Endnode {
            @display("p=488,308,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        endnode20: Endnode {
            @display("p=486,365,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

    connections:
        gateway1.json <--> lns01.json++;
        gateway2.json <--> lns01.json++;

}
