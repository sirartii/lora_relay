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

package opp_lora_relay_official;
import opp_lora_relay_official.Radio;
import opp_lora_relay_official.LogicRelay;
import opp_lora_relay_official.Distortion;


module Gateway
{
    parameters:
        @display("p=$xpos,$ypos;i=device/antennatower");
    gates:
        input radioIn @directIn;
        inout json;
     	//output radioOut @dire;
     	
    submodules:
        radio: Radio {
            @display("p=342,203");
        }
        logic: LogicGateway {
            @display("p=228,33");
        }
        distortion: Distortion {
            @display("p=72,134");
        }
    connections:
        radio.radioIn <-- radioIn;
        //        radio.radioOut --> radioOut;
        radio.fromLogic <-- logic.logicOut;
        radio.toLogic --> distortion.in;
        distortion.out --> logic.logicIn;
        
        logic.json <--> json;
}
