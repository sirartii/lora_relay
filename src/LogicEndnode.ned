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
// TODO auto-generated module
//
simple LogicEndnode
{
        parameters:
        volatile double sendInterval @unit(s) = default(exponential(60min));
        int transmittingWindowStart1;
        int transmittingWindowStart2;
        int transmittingWindowDuration1;
        int transmittingWindowDuration2;
        int missedPacketsToSearchRelay;
        int retransmissionModel;
        double clockDriftMax;
        @signal[wakeupRelay](type=double);

		int framesPerWindowMin = default(1);
		int framesPerWindowMax = framesPerWindowMin;
		int windowsPerDay = default(2);
		int forceSF = default(12);


	gates:
        output logicOut;
        input logicIn;
}
