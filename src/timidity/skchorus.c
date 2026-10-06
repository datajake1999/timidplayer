#include "skchorus.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

sk_chorus * sk_chorus_new(int sr, float delay)
{
	sk_chorus *c;
	float *buf;
	long sz;

	c = (sk_chorus *)malloc(sizeof(sk_chorus));
	sz = floor(delay * sr);
	buf = (float *)malloc(sizeof(float) * sz);
	sk_chorus_init(c, sr, buf, sz);

	return c;
}

void sk_chorus_del(sk_chorus *c)
{
	free(c->buf);
	free(c);
	c = NULL;
}

void sk_chorus_init(sk_chorus *c, int sr, float *buf, long sz)
{
	c->prate = -1;
	sk_chorus_rate(c, 0.5);
	sk_chorus_depth(c, 1);
	sk_chorus_mix(c, 0.5);
	c->sr = sr;
	c->buf = buf;
	c->sz = sz;
	c->wpos = sz - 1;
	{
		long i;
		for (i = 0; i < sz; i++) c->buf[i] = 0;
	}
	c->z1 = 0;
	c->ym1 = 0;
	{
		float b;
		float freq;

		freq = 2020;

		b = 2.0 - cos(freq * (2 * M_PI / sr));
		c->a = b - sqrt(b*b - 1);
	}
	c->mc_x[0] = 1;
	c->mc_x[1] = 0;
	c->mc_eps = 0;
}

void sk_chorus_rate(sk_chorus *c, float rate)
{
	c->rate = rate;
}

void sk_chorus_depth(sk_chorus *c, float depth)
{
	if (depth < 0) depth = 0;
	if (depth > 1) depth = 1;
	c->depth = depth;
}

void sk_chorus_mix(sk_chorus *c, float mix)
{
	c->mix = mix;
}

float sk_chorus_tick(sk_chorus *c, float in)
{
	float out;
	float lfo;
	float t;
	float frac;
	long p1, p2;
	out = 0;

	if (c->prate != c->rate) {
		c->prate = c->rate;
		c->mc_eps = 2.0 * sin(M_PI * (c->rate / c->sr));
	}

	c->mc_x[0] = c->mc_x[0] + c->mc_eps * c->mc_x[1];
	c->mc_x[1] = -c->mc_eps * c->mc_x[0] + c->mc_x[1];
	lfo = (c->mc_x[1] + 1) * 0.5;
	t = (lfo * 0.9 * c->depth + 0.05) * c->sz;
	p1 = c->wpos - (int)floor(t);
	if (p1 < 0) p1 += c->sz;
	p2 = p1 - 1;
	if (p2 < 0) p2 += c->sz;
	frac = t - (int)floor(t);
	out = c->buf[p2] + c->buf[p1]*(1 - frac) - (1 - frac)*c->z1;
	c->z1 = out;
	c->ym1 = (1 - c->a) * out + c->a*c->ym1;
	out = c->ym1;
	c->buf[c->wpos] = in;
	c->wpos++;
	if (c->wpos >= c->sz) c->wpos = 0;
	out = c->mix * out + (1 - c->mix) * in;

	return out;
}
