# README figure data and provenance

The figures on the project home page are supplied as English-language examples. This directory contains the numeric values used to draw them and a machine-readable [provenance record](provenance.json).

## Current-model characteristics

[`airtime-example.csv`](airtime-example.csv) follows `LoRaPacket::calcMsgDuration()` at source commit `c2e7f6c262dd1326f240437ad30f9111d1832364`:

- 51-byte application payload plus 13 bytes of frame overhead;
- SF7–SF12, BW 125 kHz, coding rate 4/5;
- explicit header, CRC and an 8-symbol preamble;
- low-data-rate optimisation for SF11 and SF12.

[`wor-standby-example.csv`](wor-standby-example.csv) follows `worStandbyCurrentMa()` and the reference profile in `WorEnergy.h` at the same commit:

- SF9, 4 CAD symbols, scan duration 19.145 ms;
- CAD current 3.84 mA and sleep current 5.6 µA;
- one or two WOR channels;
- `I_WOR = I_sleep + (I_CAD - I_sleep) × channels × scan_duration / CAD_period`.

The calculation covers time spent waiting in WOR. It excludes packet RX/TX and board-level offset current. Two channels at a 20 ms period would require overlapping scans, so that point is omitted as infeasible. These deterministic model calculations have no statistical confidence intervals and are not measurements of a finished device.

## Archived delivery comparison

[`delivery-example.csv`](delivery-example.csv) contains the 30 aggregate rows plotted from the saved `B11_probabilities.json` of the E-003 research archive dated 19 September 2026. Its SHA-256 is recorded in [provenance.json](provenance.json). Only the values needed for the public figure are included.

The study used one gateway, two relays, a central population and two weaker-coverage device groups. Total population ranged from 55 to 550 devices. Each point represents 30 independent 30-day replications, with four warm-up days and 26 days used for the delivery metrics. The traffic profile had two 12-hour windows per day and two scheduled opportunities per window. Direct links used SF12; the configured relay access/backhaul profile used SF9. The clock parameter was 40 ppm.

The compared series are Enhanced Proximity (`EP4312388`), TS011 with the archived forwarding-bucket configuration, and TS011 with forwarding limits disabled. The limited profile used trusted/global hourly reload rates of 8 and bucket sizes of 16; therefore “8/h/relay” is shorthand for that configured limit and does not imply a hard cap of eight packets in every arbitrary one-hour interval.

The two metrics answer different questions:

- **Daily delivery:** fraction of observed device-days with at least one uplink delivered.
- **Unique-uplink delivery:** fraction of unique generated uplinks delivered to the LNS, with retries deduplicated. The archived analysis includes Join uplinks in this metric.

The CSV contains the replication mean and the lower/upper bounds of the two-sided 95% Student-t interval of the mean. Lines in the figure connect observed population levels; they are not fitted analytical models. The vertical scales are zoomed to show differences.

This archived result predates E-005 frequency separation, E-006 dynamic WOR energy and the optional multichannel gateway extension. It also compares different configured activation policies: EP uses its missing-response threshold of 6, whereas the TS011 profile allows a confirmed-message relay fallback after one missed expected ACK. These results must not be presented as a current-branch benchmark, a framing-only comparison, or a universal ranking of relay mechanisms.

For the complete archived run set, analysis workflow or help reproducing a specific experiment, please [contact the maintainer](../../README.md#contact-and-collaboration).
