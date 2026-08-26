# HwaSimIR DDS Video Interface Control Document

This ICD covers only the D2 video-only DDS interface. Initialization, control,
annotation, target tracking realtime data, and TCP packet formats are outside
this DDS interface and remain on their existing transports.

## Middleware contract

- DDS implementation: ZRDDS, installed package path labelled 2.4.5. The tested
  runtime banner is `2.4.4-r6873577`.
- Built-in type: `DDS::Bytes`; no IDL is required.
- Domain default: `150`.
- Transport: ZRDDS `tcpv4`.
- Writer and reader reliability: `RELIABLE_RELIABILITY_QOS`.
- Writer history: `KEEP_ALL_HISTORY_QOS`.
- The supplied customer profile is
  `DDS/HwaSimIRVideoReceiverDemo/Config/ZRDDS_QOS_PROFILES.xml` and uses
  `tcpv4://default//0`.
- `BEST_EFFORT` and UDP large-package zero-copy are not permitted.

DDS provides video payload only. A Sample never contains a frame sequence,
PTS/DTS, width, height, codec identifier, platform/sensor identifier, TCP v2/v3
header, annotation, realtime data, initialization, or control data.

## Topics

| Channel | Codec/pixel format | Topic |
|---|---|---|
| precise | H.264 | `HwaSimIR.Video.precise.H264` |
| precise | Gray8 | `HwaSimIR.Video.precise.RawGray8` |
| precise | BGR24 | `HwaSimIR.Video.precise.RawBGR24` |
| coarse | H.264 | `HwaSimIR.Video.coarse.H264` |
| coarse | Gray8 | `HwaSimIR.Video.coarse.RawGray8` |
| coarse | BGR24 | `HwaSimIR.Video.coarse.RawBGR24` |

Topic and codec must agree. A JPEG payload must never be published on an H.264
topic. `DdsVideo.Codec=auto` means `trackerSensorParam.h264En=true` selects H.264
and `false` selects the configured Raw pixel format; `TcpOutput.Codec` does not
change this rule.

## H.264 Sample contract

One DDS Sample is exactly one complete H.264 Annex-B Access Unit (AU). The first
byte is the first byte of the original AU. No length prefix or other header is
inserted. An AU may contain multiple Annex-B NAL units, including SPS/PPS and an
IDR. The sender requests an IDR on initialization, reset, a new round, encoder
reset, and DDS writer/topic creation.

Receivers append Samples in receive order to reconstruct an Annex-B elementary
stream. The receiver must not parse `TcpVideoPacketV3`. Where a local display
timestamp is required, it may derive one from its own Sample counter and the
agreed FPS; that timestamp is not sender metadata.

## Raw Sample contracts

One DDS Sample is exactly one complete frame. Geometry is configured out of
band and is never transmitted in the Sample.

- `RawGray8`: tightly packed, one byte per pixel. Required Sample size is
  `width * height`.
- `RawBGR24`: tightly packed B, G, R bytes with no row padding. Required Sample
  size is `width * height * 3`.

Raw orientation matches the final HwaSimIR video output. With
`Stage6Capture/FlipInTcpThread=true`, the DDS Raw preparation applies the same
vertical flip as the TCP video path. A receiver must reject a Sample whose size
does not exactly match the configured geometry.

## Delivery and backpressure

The HwaSim_IR application queue and DDS writer queue are no-drop queues. When
full, the producer blocks; it does not clear, overwrite, pop the oldest frame,
or continue after a write failure. Reliable acceptance requires the sender's
`sentSamples` to equal the receiver's `receivedSamples`, with sender/receiver
errors and application dropped counts all zero.

On normal simulation STOP the application stops producing, drains its DDS
queue, and retains the Writer and Participant. Topic changes and final process
shutdown additionally use `wait_for_acknowledgments` plus a bounded drain. The
installed runtime's acknowledgement call is not used as the sole proof of
delivery; endpoint Sample counts are authoritative.

