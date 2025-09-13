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
// LogicIpBridge.h
#ifndef __OPP_LORA_RELAY_OFFICIAL_LOGICIPBRIDGE_H_
#define __OPP_LORA_RELAY_OFFICIAL_LOGICIPBRIDGE_H_

#include <omnetpp.h>

using namespace omnetpp;

namespace opp_lora_relay_official {

class LogicIpBridge : public cSimpleModule
{
  protected:
    bool forwardDownlink = false;

    virtual void initialize() override;
    virtual void handleMessage(omnetpp::cMessage *msg) override;
};

} // namespace opp_lora_relay_official

#endif
