# AV1 Codec Support in SparkRTC

This document describes the AV1 codec implementation in SparkRTC.

## Overview

SparkRTC supports the AV1 (AOMedia Video 1) codec, which is a modern, royalty-free video codec designed for efficient video compression. AV1 offers better compression efficiency compared to VP9 and H.264, resulting in higher quality video at lower bitrates.

## Implementation

The AV1 implementation in SparkRTC consists of:

1. **Encoder**: Uses the libaom library implementation
2. **Decoder**: Uses the dav1d library implementation

## Build Configuration

AV1 support is enabled by default in SparkRTC with the following build flags:

- `rtc_use_libaom_av1_encoder = true` - Enables the AV1 encoder using libaom
- `rtc_include_dav1d_in_internal_decoder_factory = true` - Enables the AV1 decoder using dav1d

## Usage

To use AV1 in your WebRTC application:

1. The AV1 codec is automatically registered in the default encoder and decoder factories
2. You can specify AV1 in your SDP by using the codec name "AV1"
3. For SVC (Scalable Video Coding) support, AV1 supports various scalability modes

## SVC Support

AV1 in SparkRTC supports Scalable Video Coding (SVC) with the following scalability modes:

- Temporal scalability (multiple frame rates)
- Spatial scalability (multiple resolutions)
- Combined temporal and spatial scalability

## Performance Considerations

- AV1 encoding is more computationally intensive than VP8/VP9
- Consider the target devices' capabilities when enabling AV1
- For low-powered devices, you may want to stick with VP8 or H.264

## Dependencies

- libaom: AOMedia AV1 encoder library
- dav1d: AV1 decoder library optimized for speed and correctness

## References

- [AV1 Codec Specification](https://aomediacodec.github.io/av1-spec/)
- [libaom Repository](https://aomedia.googlesource.com/aom/)
- [dav1d Repository](https://code.videolan.org/videolan/dav1d)