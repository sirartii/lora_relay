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

#ifndef __OPP_LORA_RELAY_OFFICIAL_KEEPER_H_
#define __OPP_LORA_RELAY_OFFICIAL_KEEPER_H_

#include <omnetpp.h>
#include <vector>

using namespace omnetpp;

namespace opp_lora_relay_official {
struct Node
{
    std::string name;
    int index;
};
/**
 * TODO - Generated class
 */
class Keeper : public cSimpleModule
{
public:
    void registerMe(std::string name, int index);
    std::vector<Node> returnRadioDevices();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    std::vector<Node> radioDevices;
};

} //namespace

#endif
