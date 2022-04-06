/*
 * common.h
 *
 *  Created on: Jan 5, 2022
 *      Author: webs
 */

#ifndef COMMON_H_
#define COMMON_H_

namespace opp_lora_relay_official {

struct DevType
{
    enum dt
    {
        DEVUNKNOWN, ENDNODE, RELAY, GATEWAY
    };
};

struct RadioActivity
{
    enum ra
    {
        UNKNOWN, RADIOOFF, LISTENING, RECEIVING, TRANSMITTING, TIMER
    };
};

struct EndDeviceState
{
    enum sm
    {
        UNKNOWN, INIT, UNSYNCHRONIZED, SYNCHRONIZED
    };
};


struct SelfMsgType
{
    enum sm
    {
        UNKNOWN, RADIOINIT, WINDOWTIMER, RXTIMER, TXTIMER, RXC, RX1, RX2, RXR, TXWINDOW1, TXWINDOW2
    };
};

struct MsgType
{
    enum mt
    {
        UNKNOWN, WORRELAYJOIN, WORRELAYUPLINK, JOINREQUEST, JOINACCEPT, FORWARDUPLINK, FORWARDDOWNLINK, UPLINK, DOWNLINK, ACK
    };
};

struct Coords
{
    double x, y;
};
}

#endif /* COMMON_H_ */
