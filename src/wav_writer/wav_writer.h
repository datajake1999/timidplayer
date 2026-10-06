#ifndef WAV_WRITER_H
#define WAV_WRITER_H

#include <stdio.h>
#include "wav_header.h"

typedef struct {
	char ChunkID[4];
	int size;
	int manufacturer;
	int product;
	int SamplePeriod;
	int MIDIUnityNote;
	int MIDIPitchFraction;
	int SMPTEFormat;
	int SMPTEOffset;
	int NumSampleLoops;
	int SampleData;
	int LoopID;
	int LoopType;
	int LoopStart;
	int LoopEnd;
	int LoopFraction;
	int LoopCount;
} smpl;

typedef struct {
	wav_header head;
	FILE *out;
	smpl loop;
	int LoopSet;
} WavWriter;

#ifdef __cplusplus
extern "C" {
#endif
int WavFileOpen(WavWriter *wr, const char *filename, int rate, int bits, int channels, int format);
int WavFileWrite(WavWriter *wr, void *buffer, int length);
void WavFileSetLoop(WavWriter *wr, int start, int end);
void WavFileClearLoop(WavWriter *wr);
void WavFileClose(WavWriter *wr);
#ifdef __cplusplus
}
#endif

#endif //WAV_WRITER_H