#!/bin/bash
# Convert the Flappy Bird source WAVs into the engine's CAF format
# (uncompressed little-endian 16-bit PCM, 22050 Hz, mono — same as Canabalt).
# Source WAVs: github.com/samuelcust/flappy-bird-assets/audio
AFCONVERT=afconvert
FORMAT=caff
DATA_FORMAT=LEI16
SAMPLE_RATE=22050
CHANNELS=1

for WAV in *.wav; do
    CAF=`basename -s .wav "$WAV"`.caf
    $AFCONVERT -f $FORMAT -d ${DATA_FORMAT}@${SAMPLE_RATE} -c $CHANNELS "$WAV" "$CAF"
done
