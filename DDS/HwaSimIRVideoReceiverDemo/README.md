# HwaSimIR ZRDDS Video Receiver Demo

This is the standalone customer-side D1 receiver. It is pure C++, uses the
ZRDDS built-in `DDS::Bytes` type, and does not depend on HwaSimIR headers, IDL,
`CommonData.h`, TCP packet structures, annotations, or realtime data.

## Wire contract

- Transport: ZRDDS `tcpv4`.
- Reliability: `RELIABLE_RELIABILITY_QOS`.
- History: `KEEP_ALL_HISTORY_QOS`.
- H.264: one DDS Sample is exactly one complete Annex-B Access Unit.
- `raw_gray8`: one DDS Sample is exactly one frame of `width * height` bytes.
- `raw_bgr24`: one DDS Sample is exactly one frame of `width * height * 3` bytes.
- No header, frame sequence, PTS, dimensions, codec field, TCP Packet v3,
  annotation, or realtime data is added to a DDS Sample.

The generic customer QoS uses `tcpv4://default//0`. The IP-bound XML files in
`tools/dds_d1_qos` are lab-only and must not be copied into customer code.

## Windows VS2015 Release x64

Requirements:

- Visual Studio 2015 / v140 toolset.
- `ZRDDS_HOME=F:\Programs\ZRDDS\ZRDDS-2.4.5`.
- A valid `zrddslicence.lic` in the process working directory or the SDK root,
  as required by the installed ZRDDS trial package.

The CAEP Trial runtime modifies the licence file. Run from the Receiver Demo
directory with an independent writable licence copy. When VideoDisplay and the
Receiver Demo run simultaneously, do not point both processes at the same
writable trial-licence file. Record only existence, writability, hash and
initialization status; never print the licence contents or Signature.

Build both D1 programs:

```powershell
& 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' `
  .\DDS\HwaSimIRVideoD1Smoke.sln /m /t:Build `
  /p:Configuration=Release /p:Platform=x64
```

Receive H.264:

```powershell
cd D:\HwaSimIR\DDS\HwaSimIRVideoReceiverDemo\x64\Release
.\HwaSimIRVideoReceiverDemo.exe `
  --domain 150 `
  --topic HwaSimIR.Video.precise.H264 `
  --codec h264 `
  --width 800 --height 800 `
  --qos D:\HwaSimIR\DDS\HwaSimIRVideoReceiverDemo\Config\ZRDDS_QOS_PROFILES.xml `
  --output received.h264 `
  --frames 300
```

Receive 800x800 Gray8:

```powershell
cd D:\HwaSimIR\DDS\HwaSimIRVideoReceiverDemo\x64\Release
.\HwaSimIRVideoReceiverDemo.exe `
  --domain 150 `
  --topic HwaSimIR.Video.precise.RawGray8 `
  --codec raw_gray8 `
  --width 800 --height 800 `
  --qos D:\HwaSimIR\DDS\HwaSimIRVideoReceiverDemo\Config\ZRDDS_QOS_PROFILES.xml `
  --output received.gray8 `
  --frames 300
```

## Debian VM cross-build for RK3588

```bash
cmake -S /home/linaro/userdata/HwaSimIR/DDS/HwaSimIRVideoReceiverDemo \
  -B /home/linaro/userdata/HwaSimIR/cmake-build-dds-receiver-aarch64 \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64
cmake --build /home/linaro/userdata/HwaSimIR/cmake-build-dds-receiver-aarch64 -j4
file /home/linaro/userdata/HwaSimIR/cmake-build-dds-receiver-aarch64/HwaSimIRVideoReceiverDemo
```

On RK3588, run from `/usr/ZRDDS/ZRDDS-2.4.5` and set
`LD_LIBRARY_PATH=/usr/ZRDDS/ZRDDS-2.4.5/lib`.

## CLI

Required or commonly used options:

- `--domain N`: DDS domain, 0 through 232.
- `--topic NAME`: topic name agreed with the publisher.
- `--codec h264|raw_gray8|raw_bgr24`.
- `--width N --height N`: mandatory frame dimensions for raw formats.
- `--qos FILE`: ZRDDS QoS XML.
- `--output FILE`: byte-exact output file.
- `--frames N`: stop after exactly N valid Samples.
- `--frames 0 --idle-exit-ms N`: accept an unknown-length HwaSim_IR round and
  stop after `N` milliseconds of inactivity following the first Sample. This
  mode is useful for proving that the sender queue fully drained at STOP.
- `--timeout-sec N`: inactivity/acceptance timeout.
- `--sample-delay-ms N`: test-only slow-consumer delay after each Sample,
  default `0`. Customer production runs should leave this at zero; D3 uses it
  to verify reliable no-drop/backpressure behavior when a Reader is slower
  than the Publisher.
- `--sample-delay-samples N`: delay only the first `N` Samples and then resume
  normal speed so a bounded backpressure test can drain. `0` means delay every
  Sample. Customer production runs should also leave this at zero.

The artificial delay runs after the callback has copied the Sample into owned
storage. It is deliberately a stress tool, not a recommendation for production
callbacks. D3 showed that a severe one-second callback delay can leave a large
tail inside the current vendor runtime even when the writer reports no error;
always compare endpoint counts and move expensive customer work to a separate
owned-buffer worker.

The final line reports `receivedSamples`, `receivedBytes`,
`sampleBytesMin/Avg/Max`, `receiveFps`, `ddsErrors`, and `timedOut`. A raw Sample
with an invalid size increments `ddsErrors`, is not written, and causes a
non-zero process result.

## Internal sender smoke

The sibling `DDS/HwaSimIRVideoSenderSmoke` program supports deterministic 4 KB,
1 MB, 800x800 Gray8, and indexed Annex-B AU input. Its AU-length file is a local
test-file index only: one decimal AU byte count per line. It is never placed in
the DDS Sample. The sender keeps the writer alive for `--drain-ms` because the
installed ZRDDS 2.4.4 runtime returns immediately from
`wait_for_acknowledgments` while the final large tcpv4 Sample may still be in
flight.

## Connecting to a real HwaSim_IR D2 publisher

The customer does not need an IDL file, `CommonData.h`, or any HwaSim_IR
library. The only shared contract is ZRDDS plus Domain/Topic/QoS/codec and, for
Raw, the agreed geometry.

Start HwaSim_IR from its normal `HwaSim_IR/Bin` working directory. Enable
`[DdsVideo]` in `Config/HwaSimIRRuntime.ini` or use the corresponding
`HwaSimIR...` environment overrides. Do not change the working directory to
the ZRDDS installation: HwaSim_IR must retain access to its normal `Config`,
`Shader`, `Model`, `Texture`, and `Weather` assets.

Receive a complete precise H.264 round and stop after the sender becomes idle:

```powershell
$receiver = 'D:\HwaSimIR\DDS\HwaSimIRVideoReceiverDemo\x64\Release\HwaSimIRVideoReceiverDemo.exe'
& $receiver `
  --domain 150 `
  --topic HwaSimIR.Video.precise.H264 `
  --codec h264 `
  --qos D:\HwaSimIR\DDS\HwaSimIRVideoReceiverDemo\Config\ZRDDS_QOS_PROFILES.xml `
  --output D:\Temp\received.h264 `
  --frames 0 --idle-exit-ms 2000 --timeout-sec 90
```

Receive a complete 800x800 precise Gray8 round:

```powershell
& $receiver `
  --domain 150 `
  --topic HwaSimIR.Video.precise.RawGray8 `
  --codec raw_gray8 --width 800 --height 800 `
  --qos D:\HwaSimIR\DDS\HwaSimIRVideoReceiverDemo\Config\ZRDDS_QOS_PROFILES.xml `
  --output D:\Temp\received.gray8 `
  --frames 0 --idle-exit-ms 2000 --timeout-sec 90
```

Receive 800x800 precise BGR24:

```powershell
& $receiver `
  --domain 150 `
  --topic HwaSimIR.Video.precise.RawBGR24 `
  --codec raw_bgr24 --width 800 --height 800 `
  --qos D:\HwaSimIR\DDS\HwaSimIRVideoReceiverDemo\Config\ZRDDS_QOS_PROFILES.xml `
  --output D:\Temp\received.bgr24 `
  --frames 0 --idle-exit-ms 10000 --timeout-sec 120
```

For coarse video, replace `precise` with `coarse` in the selected topic. The
receiver writes every Sample payload verbatim and never adds a file header.
