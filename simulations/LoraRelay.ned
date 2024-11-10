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
        int numEndnodes = default(5);

        @display("bgb=1102,527");
    types:
    submodules:
        keeper: Keeper {
            @display("p=136.86751,452.0775");
        }

        endnode[numEndnodes]: Endnode {
            @display("p=535.0275,73.2725,m,5,100,100");
        }

//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        gateway: Gateway {

            @display("p=410.6025,305.5325");
        }
//        endnode1[1]: Endnode {
//            @display("p=619.36005,345.625");
//        }

        relay01: Relay {
            @display("p=743.78503,305.5325");
        }

        lns01: LNS {
            @display("p=164.5175,305.5325");
        }
//
//        relay02: Relay {
//        }

    connections:
        gateway.json <--> lns01.json;
}
