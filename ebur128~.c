#include "m_pd.h"
#include <math.h>
#include "libebur128/ebur128/ebur128.h"

enum {
	BSIZE = 1 << 11,
	BSIZE_M2 = BSIZE << 1,
};

/* Inputs: 
 *   [0] -> left channel|bang
 *   [1] -> right channel
 * Outputs:
 *   [0] -> true peak left channel
 *   [1] -> true peak right channel
 *   [2] -> momentary lufs
 *   [3] -> short-term lufs
 */
static t_class *ebur128_tilde_class;
typedef struct ebur128_tilde {
	t_object x_obj;
	t_sample f;
	unsigned long sr;
	ebur128_state *st;
	t_sample buffer[BSIZE_M2];
	t_sample out[4];
	unsigned int pos;
	t_inlet *x_in2;
	t_outlet *x_out1;
	t_outlet *x_out2;
	t_outlet *x_out3;
	t_outlet *x_out4;
} t_ebur128_tilde;

void *ebur128_tilde_new() {
	t_ebur128_tilde *x = (t_ebur128_tilde *)pd_new(ebur128_tilde_class);
	
	x->sr = 44100UL;
	if(!(x->st = ebur128_init(2, x->sr, EBUR128_MODE_TRUE_PEAK | EBUR128_MODE_S))) {
		error("Out of memory?");
	}

	x->pos = 0;
	x->x_in2 = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
	x->x_out1 = outlet_new(&x->x_obj, &s_float);
	x->x_out2 = outlet_new(&x->x_obj, &s_float);
	x->x_out3 = outlet_new(&x->x_obj, &s_float);
	x->x_out4 = outlet_new(&x->x_obj, &s_float);
	return (void *)x;
}

void ebur128_tilde_reset(t_ebur128_tilde *x) {
	ebur128_state *st = ebur128_init(2, x->sr, EBUR128_MODE_TRUE_PEAK | EBUR128_MODE_S);
	if(x->st) ebur128_destroy(&x->st);
	x->st = st;
}

inline float lin2db(float x) {
	return 20.f * log10f(x);
}

void ebur128_tilde_bang(t_ebur128_tilde *x) {
	double out[4];

	ebur128_true_peak(x->st, 0, &out[0]);
	ebur128_true_peak(x->st, 1, &out[1]);
	ebur128_loudness_momentary(x->st, &out[2]);
	ebur128_loudness_shortterm(x->st, &out[3]);
	outlet_float(x->x_out1, lin2db((float)out[0]));
	outlet_float(x->x_out2, lin2db((float)out[1]));
	outlet_float(x->x_out3, (float)out[2]);
	outlet_float(x->x_out4, (float)out[3]);
	return;
}

void ebur128_tilde_free(t_ebur128_tilde *x) {
	if(x->st) ebur128_destroy(&x->st);
	inlet_free(x->x_in2);
	outlet_free(x->x_out1);
	outlet_free(x->x_out2);
	outlet_free(x->x_out3);
	outlet_free(x->x_out4);
}

inline static void tick(t_ebur128_tilde *x, t_sample left, t_sample right) {
	if(x->pos == BSIZE) {
		x->pos = 0;
		ebur128_add_frames_float(x->st, x->buffer, BSIZE);
	}

	x->buffer[2*x->pos] = left;
	x->buffer[2*x->pos + 1] = right;
	x->pos++;
	return;
}

t_int *ebur128_tilde_perform(t_int *w) {
	t_ebur128_tilde *x = (t_ebur128_tilde *)(w[1]);
	t_sample *in1 = (t_sample *)(w[2]);
	t_sample *in2 = (t_sample *)(w[3]);
	int n = (int)(w[4]);

	for(int i = 0; i < n; i++) {
		tick(x, in1[i], in2[i]);
	}

	return (w + 5);
}

void ebur128_tilde_dsp(t_ebur128_tilde *x, t_signal **sp) {
	unsigned long sr = (unsigned long)sp[0]->s_sr;
	if(sr != x->sr) {
		ebur128_change_parameters(x->st, 2, sr);
		x->sr = sr;
	}

	dsp_add(ebur128_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void ebur128_tilde_setup(void) {
	ebur128_tilde_class = class_new(gensym("ebur128~"),
			(t_newmethod)ebur128_tilde_new,
			(t_method)ebur128_tilde_free,
			sizeof(t_ebur128_tilde),
			CLASS_DEFAULT,
			0);
	class_addmethod(ebur128_tilde_class, (t_method)ebur128_tilde_dsp, gensym("dsp"), 0);
	class_addmethod(ebur128_tilde_class, (t_method)ebur128_tilde_reset, gensym("reset"), 0);
	class_addmethod(ebur128_tilde_class, (t_method)ebur128_tilde_bang, gensym("bang"), 0);
	CLASS_MAINSIGNALIN(ebur128_tilde_class, t_ebur128_tilde, f);
}
