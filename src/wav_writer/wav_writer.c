#include "wav_writer.h"
#include <string.h>

static int fileexists(const char * filename)
{
	FILE *check;
	if (check = fopen(filename, "r"))
	{
		fclose(check);
		return 1;
	}
	return 0;
}

int WavFileOpen(WavWriter *wr, const char *filename, int rate, int bits, int channels, int format)
{
	if (!wr)
	{
		return 0;
	}
	if (wr->out)
	{
		return 0;
	}
	else
	{
		if (fileexists(filename))
		{
			return 0;
		}
		wr->out = fopen(filename, "wb");
		if (!wr->out)
		{
			return 0;
		}
	}
	memset(&wr->head, 0, sizeof(wr->head));
	strncpy(wr->head.riff_header, "RIFF", 4);
	wr->head.wav_size = sizeof(wr->head)-8;
	strncpy(wr->head.wave_header, "WAVE", 4);
	strncpy(wr->head.fmt_header, "fmt ", 4);
	wr->head.fmt_chunk_size = 16;
	wr->head.audio_format = format;
	wr->head.num_channels = channels;
	wr->head.sample_rate = rate;
	wr->head.byte_rate = bits / 8 * channels * rate;
	wr->head.sample_alignment = bits / 8 * channels;
	wr->head.bit_depth = bits;
	strncpy(wr->head.data_header, "data", 4);
	wr->head.data_bytes = 0;
	if (!fwrite(&wr->head, sizeof(wr->head), 1, wr->out))
	{
		fclose(wr->out);
		wr->out = NULL;
		memset(&wr->head, 0, sizeof(wr->head));
		return 0;
	}
	return 1;
}

int WavFileWrite(WavWriter *wr, void *buffer, int length)
{
	if (!wr || !wr->out || !buffer)
	{
		return 0;
	}
	return fwrite(buffer, wr->head.sample_alignment, length, wr->out);
}

void WavFileSetLoop(WavWriter *wr, int start, int end)
{
	if (!wr || !wr->out)
	{
		return;
	}
	strncpy(wr->loop.ChunkID, "smpl", 4);
	wr->loop.size = sizeof(wr->loop)-8;
	wr->loop.NumSampleLoops = 1;
	wr->loop.LoopStart = start;
	wr->loop.LoopEnd = end;
	wr->LoopSet = 1;
}

void WavFileClearLoop(WavWriter *wr)
{
	if (!wr || !wr->out)
	{
		return;
	}
	wr->LoopSet = 0;
	memset(&wr->loop, 0, sizeof(wr->loop));
}

void WavFileClose(WavWriter *wr)
{
	int size;
	if (!wr || !wr->out)
	{
		return;
	}
	size = ftell(wr->out);
	wr->head.wav_size = size-8;
	wr->head.data_bytes = size-sizeof(wr->head);
	if (wr->LoopSet)
	{
		fwrite(&wr->loop, sizeof(wr->loop), 1, wr->out);
		wr->head.wav_size += sizeof(wr->loop);
	}
	fseek(wr->out, 0, SEEK_SET);
	fwrite(&wr->head, sizeof(wr->head), 1, wr->out);
	fclose(wr->out);
	wr->out = NULL;
	memset(&wr->head, 0, sizeof(wr->head));
	memset(&wr->loop, 0, sizeof(wr->loop));
	wr->LoopSet = 0;
}
