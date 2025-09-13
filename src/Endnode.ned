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
import opp_lora_relay_official.LogicEndnode;
import opp_lora_relay_official.Distortion;


module Endnode
{
    parameters:
        double xpos = default(100);
        double ypos = default(100);
        @display("p=$xpos,$ypos;i=device/palm;is=vs");
    gates:
        input radioIn @directIn;
    //	    output radioOut;
    submodules:
        radio: Radio {
            @display("p=374,249");
        }
        logic: LogicEndnode {
            @display("p=35,28");
        }
        distortion: Distortion {
            @display("p=178,171");
        }
    connections:
        radio.radioIn <-- radioIn;
        //        radio.radioOut --> radioOut;
        radio.fromLogic <-- logic.logicOut;
        radio.toLogic --> distortion.in;
        distortion.out --> logic.logicIn;
}
