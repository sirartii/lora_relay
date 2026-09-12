# TS011-1.0.1 implementation scope

This branch implements the Class A, single-hop relay mechanisms needed for a
comparative TS011 versus EP431 study. The normative source is
`docs/ts011-1-0-1-relay.pdf` (September 2024).

## Implemented mechanisms

| TS011 area | Simulator implementation |
|---|---|
| Channel scan (3.2.1) | A relay evaluates overlap between the variable WOR preamble and periodic CAD scan instants. With two WOR channels enabled, scans alternate with a half-period offset. |
| Default/second channel (3.2.2-3.2.3) | Initialized and unsynchronized devices use the default channel. Synchronized devices prefer the configured second channel. Data, relay-backhaul, RX2 and RXR channels are independently configurable. |
| WOR reception and timing (3.2.4, 3.7) | WOR carries the subsequent uplink channel and data rate. Join WOR uses the no-ACK timing; Class A WOR reserves the WOR ACK airtime even when the ACK is missed. |
| WOR ACK (3.3, 6) | The relay returns CadToRx, forwarding status, relay data rate, crystal accuracy, CAD periodicity and TOffset. ACK loss can be configured probabilistically. |
| Synchronization (3.9, Appendix 1) | Initialized, unsynchronized and synchronized states are modeled. TREF, TNEXT, drift error and variable preamble length follow the specification formulas. Eight missed WOR ACKs trigger the specified state transitions. |
| RXR (3.6, 7) | RXR opens only after a relayed exchange and only when neither RX1 nor RX2 delivered a valid downlink. The relay forwards at the configured RXR delay. |
| Uplink encapsulation (8.1, 9.1) | ForwardUplinkReq uses FPort 226 and adds the 6-byte metadata block plus the encapsulated PHYPayload overhead. RSSI, SNR, frequency/channel and data-rate metadata are preserved separately from the relay backhaul measurement. |
| Forwarding timing (8.2) | The relay transmits the encapsulated uplink exactly `ts011RelayForwardDelay` after completing access-link reception. The original uplink end time is retained for RXR and network-time correction. |
| Payload limits (8.3, 8.5) | The application payload stays fixed. Only TS011 encapsulation changes frame size. Frames exceeding `ts011MaximumForwardedPayload` are discarded. |
| Downlink encapsulation (8.4, 9.2) | ForwardDownlinkReq is accepted only on FPort 226 by the relay that originated the forwarded uplink, then decapsulated and transmitted in RXR. |
| Forwarding limitations (8.8) | Join, per-device, aggregate trusted-device and global token buckets use hourly discrete reloads, configured sizes, no-limit mode, and forwarding status in WOR ACK. |
| Single hop | A relay rejects already-relayed uplinks, preventing relay-to-relay forwarding. |

WOR MIC/content validity is represented by `ts011WorValidityProbability`; the
radio model does not execute AES because the simulator has no byte-level key or
DevAddr session model. Successful validity represents the combined decrypt,
content and MIC checks. Join filtering and all runtime Relay MAC command
configuration are intentionally outside the requested scope.

## Deliberate exclusions

- Class B and Class C behavior
- multihop relaying
- Relay MAC commands and dynamic network-controlled configuration
- byte-level cryptography and key provisioning
- variable application payload generation

## Experiment configurations

`simulations/omnetpp.ini` contains:

- `TS011_Default`: one WOR channel and 1 s CAD periodicity
- `TS011_DualChannel`: two WOR channels and 500 ms CAD periodicity
- `TS011_ParameterSweep`: 48 combinations of CAD periodicity, channel count and relay clock accuracy
- `TS011_EnvironmentSweep`: path-loss variation and receiver-threshold sweep
- `EP431_Reference`: matching topology and traffic with the EP431 access mechanism

All TS011 parameters are defined in the General section so a new experiment can
override one value without editing NED or C++ sources.
