/*
 * common.h
 *
 *  Created on: Jan 5, 2022
 *      Author: webs
 */

#ifndef COMMON_H_
#define COMMON_H_

namespace opp_lora_relay_official {

const int REPLAY_TIMEOUT = 15;        //time in seconds from TX start to logcial no response detection
const int WOR_DATA_DELAY = 5;
const int WOR_DATA_LENGTH = 5;      //1+4 or 1+14 according to TS011 p.22
const int RELAY_FWD_DELAY = 1;
const int WOR_TO_UPLINK_TIMEOUT = 2;   //max czas aktywno�ci radia po odebraniu WOR do odebrania UPLINK

const int RELAY_JOIN_LENGTH = 10;

enum DevType
{
    DEVUNKNOWN, ENDNODE, RELAY, GATEWAY, IPBRIDGE
};

enum RetransmissionModel
{
    UNKNOWN,
    TS011,
    EP431
};

struct RadioActivity
{
    enum ra
    {
        UNKNOWN,
        RADIOOFF,
        LISTENING,
        WOR,            //Wake up on radio TS011
        EWOR,           //Wake up on radio M&T
        RECEIVING,
        TRANSMITTING,
        TIMER
    };
};

enum EndDeviceState
{
    OFFLINE,  //also init
    JOIN_DIRECT,
    JOIN_RELAY,
    ONLINE_DIRECT,
    ONLINE_RELAYED,
};


struct SelfMsgType
{
    enum sm
    {
        UNKNOWN, RADIOINIT, WINDOWTIMER, RXTIMER, TXTIMER, RXC, RX1, RX2, RXR, TXWINDOW1, TXWINDOW2, TXREPEAT
    };
};

enum MsgType
{
    MSGUNKNOWN,
    MSGWOR,
    //WORRELAYJOIN,
    //WORRELAYUPLINK,
    //WORRELAYJOINACCEPT,
    JOINREQUEST,
    JOINACCEPT,
    FORWARDUPLINK,
    FORWARDDOWNLINK,
    UPLINK,
    DOWNLINK,
    ACK,
    TIMEOUT
};

struct Coords
{
    double x, y;
};
}

#endif /* COMMON_H_ */
