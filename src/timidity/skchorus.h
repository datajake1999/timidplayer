#ifndef SK_CHORUS_H
#define SK_CHORUS_H

typedef struct {
	float rate, prate;
	float depth;
	float mix;
	int sr;
	float *buf;
	long sz;
	long wpos;
	float z1;
	float ym1;
	float a;
	float mc_x[2];
	float mc_eps;
} sk_chorus;

#ifdef __cplusplus
extern "C" {
#endif

sk_chorus * sk_chorus_new(int sr, float delay);
void sk_chorus_del(sk_chorus *c);
void sk_chorus_init(sk_chorus *c, int sr, float *buf, long sz);
void sk_chorus_rate(sk_chorus *c, float rate);
void sk_chorus_depth(sk_chorus *c, float depth);
void sk_chorus_mix(sk_chorus *c, float mix);
float sk_chorus_tick(sk_chorus *c, float in);

#ifdef __cplusplus
}
#endif

#endif
