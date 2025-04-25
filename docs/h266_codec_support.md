# H.266/VVC Codec Support in SparkRTC

This document describes the H.266/VVC (Versatile Video Coding) codec support in SparkRTC.

## Overview

H.266/VVC is the successor to H.265/HEVC, offering approximately 50% better compression efficiency compared to H.265 for the same perceptual quality. It was developed by the Joint Video Experts Team (JVET) of ITU-T VCEG and ISO/IEC MPEG and was finalized in July 2020.

SparkRTC includes experimental support for H.266/VVC encoding and decoding using the following libraries:
- VVenC (Fraunhofer Versatile Video Encoder) for encoding
- VVdeC (Fraunhofer Versatile Video Decoder) for decoding

## Features

- Support for H.266 Main profile
- Hardware acceleration (where available)
- Integration with WebRTC's video codec framework
- Support for key frame requests and frame dropping
- Configurable bitrate, framerate, and resolution

## Build Configuration

H.266 support is disabled by default. To enable it, you need to set the following GN build flags:

```
rtc_use_vvenc_h266_encoder = true
rtc_use_vvdec_h266_decoder = true
```

You can set these flags in your `args.gn` file or pass them to the `gn gen` command:

```bash
gn gen out/Default --args='rtc_use_vvenc_h266_encoder=true rtc_use_vvdec_h266_decoder=true'
```

## Dependencies

To build with H.266 support, you need to have the following libraries installed:

- VVenC (Fraunhofer Versatile Video Encoder)
- VVdeC (Fraunhofer Versatile Video Decoder)

## Usage

### SDP Negotiation

H.266 is negotiated using the codec name "H266" in SDP. Example SDP:

```
m=video 9 UDP/TLS/RTP/SAVPF 96
a=rtpmap:96 H266/90000
```

### API Usage

To use H.266 in your application, you can create an encoder or decoder factory that includes H.266 support:

```cpp
// Create a video encoder factory with H.266 support
std::unique_ptr<VideoEncoderFactory> CreateEncoderFactory() {
  return std::make_unique<InternalEncoderFactory>();
}

// Create a video decoder factory with H.266 support
std::unique_ptr<VideoDecoderFactory> CreateDecoderFactory() {
  return std::make_unique<InternalDecoderFactory>();
}
```

## Limitations

- H.266 is a relatively new codec and may not be supported by all platforms and browsers
- Hardware acceleration may not be available on all platforms
- The implementation is experimental and may have performance limitations
- Requires the VVenC and VVdeC libraries to be installed on the system

## Future Work

- Improve performance and stability
- Add hardware acceleration support
- Optimize for low-latency real-time communication
- Add support for scalable video coding (SVC) with H.266
- Add support for more H.266 profiles and levels

## References

- [H.266/VVC Standard](https://www.itu.int/rec/T-REC-H.266)
- [VVenC: Fraunhofer Versatile Video Encoder](https://github.com/fraunhoferhhi/vvenc)
- [VVdeC: Fraunhofer Versatile Video Decoder](https://github.com/fraunhoferhhi/vvdec)
- [WebRTC.org](https://webrtc.org/)