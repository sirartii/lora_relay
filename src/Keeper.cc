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

#include "Keeper.h"

namespace opp_lora_relay_official {

Define_Module(Keeper);

void Keeper::initialize()
{
    // TODO - Generated method body
}

void Keeper::registerMe(std::string name, int index)
{
   Node n;
   n.name = name;
   n.index = index;
   radioDevices.push_back(n);
   EV<<name << "\n";
}

std::vector<Node> Keeper::returnRadioDevices()
{
   return radioDevices;
}

void Keeper::handleMessage(cMessage *msg)
{
    // TODO - Generated method body
}

} //namespace
