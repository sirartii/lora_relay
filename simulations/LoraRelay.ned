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
network LoraRelay
{
    parameters:
        int numEndnodes = default(15);

        @display("bgb=1685.2676,526.73254");
    types:
    submodules:
        keeper: Keeper {
            @display("p=136.86751,452.0775;r=20,#FFFF80,#FF8080,2");
        }

        endnode[numEndnodes]: Endnode {
            @display("p=294.4725,59.447502,m,5,100,100;r=1,#FF80FF,#0080FF,2;t=,t,#0080FF");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        gateway: Gateway {

            @display("p=53.917503,262.67502;r=1,#FF8000,black,2");
        }
//        endnode1[1]: Endnode {
//            @display("p=619.36005,345.625");
//        }

        relay01: Relay {
            @display("p=608.04,261.795;r=1,#0080C0,#FF0080,2;t=test decoration");
        }

        lns01: LNS {
            @display("p=53,91");
        }
//
//        relay02: Relay {
//        }

        ipb1: IpBridge {
            @display("p=1123,108");
        }
    connections:
        gateway.json <--> lns01.json++;
        ipb1.json <--> lns01.json++;
}
