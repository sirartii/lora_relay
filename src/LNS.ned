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
// module collecting data from multiple Gateways. Contains packet routing and relay management.
//
simple LNS
{
    parameters:

    	@display("i=device/router;i2=status/bulb");
    	double dedupHoldoff @unit(s) = default(0.1s);
        double listeningWindowRX1; //listening window 1 duration in seconds
        double listeningWindowRX2; //listening window 2 duration in seconds
        double listeningWindowRXR; //listening relay window duration in seconds
        double listeningWindowRXC; //time between transmission and RX1, and beetween RX1 and RX2, in seconds
        double listeningWindowRXCR; //time beetween RX2 and RXR, in seconds

    gates:
       inout json[];	//wektor do połaczenia wielu Gateway
}
