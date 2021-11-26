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

//
// Generates messages with a configurable interarrival time.
//
simple Radio
{
    parameters:
        @display("i=block/source");
        double txPower = default(20);
        double sensitivity = default(-115);
        int listeningWindowStart1;
        int listeningWindowStart2;
        int listeningWindowDuration1;
        int listeningWindowDuration2;
        
    gates:
//        output radioOut;
        input radioIn;
        output toLogic;
        input fromLogic;
        
}
