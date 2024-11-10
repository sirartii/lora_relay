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

#include "LogicRelay.h"
#include "common.h"

namespace opp_lora_relay_official {

Define_Module(LogicRelay);

void LogicRelay::initialize() {
    //baseNode = base;
    // TODO - Generated method body
}

void LogicRelay::handleMessage(cMessage *msg) {
    if (msg->getSenderModule() == this) {
        std::cout << this->getName() << " discarding self message";
    }else if(msg->getKind() == (short)MsgType::WORRELAYJOIN) {
        if (hasGUI()) {
            char text[32];
            sprintf(text, "Retransmission ", msg->getSenderModuleId());
            this->getParentModule()->bubble(text);
        }

        send(msg,"logicOut");
    }else if (msg->getSenderGateId() == endNodeID) {
        send(msg,"logicOut");
        // TODO - retransmitt
    }
    else {
        if (msg->arrivedOn("logicIn")) {
            delete msg;
        }
    }
}

} //namespace
