#ifndef COMMON_H_
#define COMMON_H_

namespace opp_lora_relay_official {

constexpr int REPLAY_TIMEOUT = 15;
constexpr int TS011_JOIN_WOR_LENGTH = 5;
constexpr int TS011_UPLINK_WOR_LENGTH = 15;
constexpr int TS011_WOR_ACK_LENGTH = 7;
constexpr int TS011_UPLINK_METADATA_LENGTH = 6;
constexpr int LORAWAN_FRAME_OVERHEAD = 13;
constexpr int RELAY_JOIN_LENGTH = 10;
constexpr int LA_FPORT_RELAY = 226;

enum DevType { DEVUNKNOWN, ENDNODE, RELAY, GATEWAY, IPBRIDGE };
enum RetransmissionModel { UNKNOWN, TS011, EP431 };
enum class Ts011SyncState { INITIALIZED, UNSYNCHRONIZED, SYNCHRONIZED };
enum class WorType { RELAY_JOIN_REQUEST = 0, CLASS_A_UPLINK = 1 };

struct RadioActivity
{
    enum ra { UNKNOWN, RADIOOFF, LISTENING, WOR, EWOR, RECEIVING, TRANSMITTING, TIMER };
};

enum EndDeviceState { OFFLINE, JOIN_DIRECT, JOIN_RELAY, ONLINE_DIRECT, ONLINE_RELAYED };

struct SelfMsgType
{
    enum sm {
        UNKNOWN, RADIOINIT, WINDOWTIMER, RXTIMER, TXTIMER, RXC, RX1, RX2, RXR,
        TXWINDOW1, TXWINDOW2, TXREPEAT, RELAY_FORWARD, RELAY_RX_TIMEOUT,
        WORACK_RX_OPEN, WORACK_RX_CLOSE, TS011_SEND_WOR, TS011_SEND_DATA,
        RESPONSE_TIMEOUT
    };
};

enum MsgType
{
    MSGUNKNOWN,
    MSGWOR,
    MSGWORACK,
    JOINREQUEST,
    JOINACCEPT,
    FORWARDUPLINK,
    FORWARDDOWNLINK,
    UPLINK,
    DOWNLINK,
    ACK,
    TIMEOUT,
    RESET_ADR
};

struct Coords { double x, y; };

} // namespace opp_lora_relay_official

#endif
