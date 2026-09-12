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
import opp_lora_relay_official.IpBridgeRadio;
import opp_lora_relay_official.LogicIpBridge;

module IpBridge
{
    parameters:
        double xpos = default(100);
        double ypos = default(100);
        @display("i=device/antennatower;bgb=502,412");
    gates:
        input radioIn @directIn; // wejście radiowe (odbiór LoRa)
        inout json;    // idealne IP do LNS

    submodules:
        radio: IpBridgeRadio {
            @display("p=368,347");
        }
        logic: LogicIpBridge {
            @display("p=105,99");
        }
    connections:
        radio.radioIn <-- radioIn;
        radio.toLogic --> logic.logicIn;

        // logika -> radio (obecnie nieużywane; zostawiamy dla kompatybilności)
        radio.fromLogic <-- logic.logicOut;

        // „IP” bez opóźnień i strat (bez kanału => 0 delay)
        logic.json <--> json;
}
