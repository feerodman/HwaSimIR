# Independent customer TX receiver

Windows Qt 5.12.12 / MinGW 7.3, ZRDDS and FFmpeg receiver for exactly `TX/HD264`, `TX/LD264`, `TX/HDecode`, `TX/LDecode`. It does not link HwaSim_IR or use its private frame SEI, Status, Meta, or Annotation Topics. Two processes can display the two channels concurrently.

From `D:\HwaSimIR`:

```powershell
./tools/p18_build_customer_receiver.ps1 -BuildDirectory build-p18-customer-receiver-v4
```

The build script names the local Qt, MinGW, ZRDDS and FFmpeg paths and deploys their runtime libraries. A valid local ZRDDS licence is required; it is excluded from the delivery manifest. The renderer retains its separate VS2015/AArch64 toolchains.

Run from the release directory containing the licence:

```powershell
./HwaSimIRCustomerTxVideoReceiverDemo.exe --topic TX/LDecode --codec raw --pixel-format gray8 --width 320 --height 320 --fps 60 --domain 150 --qos ZRDDS_PROTOCOL_QOS.xml --output D:/HwaSimIR/logs/p18/manual/coarse --display 1 --idle-ms 12000 --timeout-ms 210000 --ffmpeg ./ffmpeg.exe
./HwaSimIRCustomerTxVideoReceiverDemo.exe --topic TX/HD264 --codec h264 --pixel-format gray8 --width 800 --height 800 --fps 60 --domain 150 --qos ZRDDS_PROTOCOL_QOS.xml --output D:/HwaSimIR/logs/p18/manual/precise --display 1 --idle-ms 12000 --timeout-ms 210000 --ffmpeg ./ffmpeg.exe
```

Use separate shells and new output directories. Raw is top-down, tightly packed Gray8 or RGB24 (R,G,B); dimensions/format must be explicit. One raw Sample is one frame, at most 640000 bytes. Compressed Samples are continuous Annex-B bytes, never individually assumed to be frames. The decoder retains partial NAL/reference state across reliable transport timing gaps; a fresh receiver waits for SPS/PPS+IDR. Invalid samples, queue overflow, storage failures and post-recovery decode errors produce a nonzero exit and explicit summary failure.

The default receive queue is bounded at 32 MiB. DDS callbacks only validate/copy/enqueue. Decoder, audit and save work runs in the worker. FFmpeg SHA256 preserves stream, sample and pixel audits. `--slow-ms`, `--callback-block-ms` are fault-test options and default to zero.

Outputs: original `.h264` / `.gray8` / `.rgb24`, `receive.jsonl`, `summary.json`, first/last PNG and `output.mp4`. Check actual saved frame count with ffprobe. Received bytes, decoded frames, GUI updates and saved frames are different measurements. No custom EOS exists; the configured local idle deadline flushes the final frame and is not an 80 ms final-frame guarantee.

See [customer ICD](../../docs/HwaSimIR_Customer_TX_DDS_ICD.md), [dual runbook](../../docs/HwaSimIR_P18_DualChannel_Runbook.md), [actual acceptance](../../docs/HwaSimIR_P18_Closeout.md). Self-written Demo interoperability does not establish interoperability with the actual customer's SDK/application. Linux receiver build is NOT_VERIFIED; the renderer's AArch64 build is a different artifact.
