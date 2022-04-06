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

import opp_lora_relay_official.Endnode;
import opp_lora_relay_official.Relay;
import opp_lora_relay_official.Gateway;
import opp_lora_relay_official.Keeper;

//
// Sample network, consisting of a source and a sink.
//
network LoraRelay
{
    @display("bgb=1102,527");
    submodules:
        keeper: Keeper {
            @display("p=44.24,452.0775");
        }
        
        endnode[1]: Endnode {
        }
        
//        relay01: Relay {
//        }
//
//        relay02: Relay {
//        }

        gateway: Gateway {
        }

    connections:

}
