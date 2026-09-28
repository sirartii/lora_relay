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
//
simple Keeper
{
    parameters:
        bool exportFrameRecords = default(false);
        bool epDesignationRestricted = default(false); // experimental: only endnode1/endnode2
        bool ts011WhitelistRestricted = default(false); // experimental: only endnode1/endnode2
    	@display("i=block/broadcast");
    	
		int numEndnodes;
		double sensitivityAllRadios;
		int enableKeeperDistortion;
		int missedPacketsToSearchRelay;
		int retransmissionModel;
		double clockDriftMax;
}
