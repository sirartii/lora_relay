# TS011-1.0.1 implementation scope

This branch implements the Class A, single-hop relay mechanisms needed for a
comparative TS011 versus EP431 study. The normative source is
`docs/ts011-1-0-1-relay.pdf` (September 2024).

## E-003 correction (2026-09-19)

This is a research subset, not a certified full TS011 stack. Corrected shared
MAC handling serializes ED transactions and associates responses with device,
message and attempt identities. Only every fifth data message requests ACK;
Join and retransmissions do not advance this counter. Session membership is
independent of the selected direct/relay data route.

The reviewer-response profile uses `ts011MissedAcksToRelay=1`: after the first
missing expected data ACK, a joined ED retries that same confirmed message
once via relay, with 1-3 s random backoff. This is an author-selected policy,
not the normative controlled-mode threshold options of 8/16/32/64. Unjoined
EDs use relay every fourth JoinRequest (`ts011JoinRelayEvery=4`, an informative
example from TS011). EP retains the scenario threshold K=6. Consequently the
comparison includes differences in route-activation policy, not only framing.

Data RX1/RX2 start 1/2 s and Join windows 5/6 s after actual TX completion.
Successful RX1 cancels RX2. TS011 WOR ACK, subsequent UL and relay forwarding
delays default to 50 ms; RXR starts after 18 s. EP keeps scenario RXR=7 s.
Expired scheduled transmissions are discarded, not silently sent late. Busy
radio time is not allowed to move a nominal window's end into the future.
Control lengths describe complete PHY payloads (WOR ACK=7 B, JoinRequest=23 B,
JoinAccept=17 B, empty ACK=12 B); data and forwarding envelopes add their own
headers. The simulation resolution remains 1 ms.

WOR Join does not expect WOR ACK. A time-bounded, transaction-specific access
grant follows Join filtering and limit admission, with another limit check
at forwarding. Forward=1/2 causes 30/60-minute relay holdoff; Forward=3 disables
further relay data access. During holdoff this profile uses direct opportunities,
retains the session and does not queue a report for an hour. Runtime MAC
reconfiguration, including clearing disabled status, remains out of scope.

`Keeper.exportFrameRecords=true` exports per-message diagnostics after finish.
Unique-UL delivery includes Join and deduplicates retries; it is not first-radio-
attempt success probability. Join attempts consume scheduled opportunities in
this model; there is no independent sensor-data backlog before successful Join.
The original EP event-based wakeup and the radio propagation/collision abstractions
are retained. See `docs/ERRATA.md`, E-003, for validation and scope limitations.

## Implemented mechanisms

### E-005 frequency separation (2026-09-20)

Default profiles now use logical frequency IDs 10/11 for first/second WOR,
and ID 1 for ordinary LoRaWAN (direct UL, post-WOR UL and relay backhaul).
These are not regional-plan channel indices. Only equal IDs interfere;
different IDs represent assumed non-overlapping frequencies. The single RF
chain remains half-duplex: frequency isolation never enables simultaneous
reception. The old relay-backhaul ID 3 was changed to 1 to keep it competing
with direct LoRaWAN traffic after introducing frequency filtering.

WOR preambles wait for explicit CAD events, at nT or (n+1/2)T. A future scan
does not lock the receiver at preamble arrival. Busy scans are skipped without
shifting phase, and acquisition must leave the required preamble symbols.
The original on-air end is preserved. Same-frequency arrivals retain collision
and capture; other-frequency preambles may survive a busy interval but cannot
interrupt an active reception. Class-A/data listening also blocks other-channel
CAD. `TS011 CAD busy skips` counts blocked per-preamble candidates, not distinct
hardware scan slots; acquisition counters count reception starts, not deliveries.

This profile models the frequency-separated option; TS011 also allows a second
channel differing only in data rate, which is not implemented here. WOR ACK
still shares its WOR frequency in this abstraction (no regional ACK offset).
CAD is a point event, not a calibrated hardware scan-duration model. The legacy
`ts011ChannelScanSymbols` input no longer permits backwards acquisition before
a scan event. Energy remains the existing constant-WOR-current approximation;
the 20 ms/two-channel setting is not a validated hardware operating point.
There is no eight-frequency gateway/demodulator bank or adjacent-channel model.
See E-005 in `ERRATA.md` for tests and consequences for older experiment results.

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
| Join filtering (8.6) | Up to 16 static forward/filter rules use longest-prefix matching over the over-air JoinEUI+DevEUI byte sequence. Rules are configured with `ts011JoinFilterRules`. |
| Forwarding limitations (8.8) | Join, per-device, aggregate trusted-device and global token buckets use hourly discrete reloads, configured sizes, no-limit mode, and forwarding status in WOR ACK. |
| Single hop | A relay rejects already-relayed uplinks, preventing relay-to-relay forwarding. |

WOR MIC/content validity is represented by `ts011WorValidityProbability`; the
radio model does not execute AES because the simulator has no byte-level key or
DevAddr session model. Successful validity represents the combined decrypt,
content and MIC checks. All runtime Relay MAC command configuration is
intentionally outside the requested scope.

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
