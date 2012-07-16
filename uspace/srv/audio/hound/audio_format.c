/*
 * Copyright (c) 2012 Jan Vesely
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup audio
 * @brief HelenOS sound server
 * @{
 */
/** @file
 */

#include <assert.h>
#include <byteorder.h>
#include <errno.h>
#include <macros.h>
#include <stdio.h>

#include "audio_format.h"

#define uint8_t_le2host(x) (x)
#define host2uint8_t_le(x) (x)
#define uint8_t_be2host(x) (x)
#define host2uint8_t_be(x) (x)

#define int8_t_le2host(x) (x)
#define host2int8_t_le(x) (x)

#define int16_t_le2host(x) uint16_t_le2host(x)
#define host2int16_t_le(x) host2uint16_t_le(x)

#define int32_t_le2host(x) uint32_t_le2host(x)
#define host2int32_t_le(x) host2uint32_t_le(x)

#define int8_t_be2host(x) (x)
#define host2int8_t_be(x) (x)

#define int16_t_be2host(x) uint16_t_be2host(x)
#define host2int16_t_be(x) host2uint16_t_be(x)

#define int32_t_be2host(x) uint32_t_be2host(x)
#define host2int32_t_be(x) host2uint32_t_be(x)

// TODO float endian?
#define float_le2host(x) (x)
#define float_be2host(x) (x)

#define host2float_le(x) (x)
#define host2float_be(x) (x)

#define from(x, type, endian) (float)(type ## _ ## endian ## 2host(x))
#define to(x, type, endian) (float)(host2 ## type ## _ ## endian(x))

static float get_normalized_sample(const void *buffer, size_t size,
    unsigned frame, unsigned channel, const audio_format_t *f);

bool audio_format_same(const audio_format_t *a, const audio_format_t* b)
{
	assert(a);
	assert(b);
	return
	    a->sampling_rate == b->sampling_rate &&
	    a->channels == b->channels &&
	    a->sample_format == b->sample_format;
}

int audio_format_mix(void *dst, const void *src, size_t size, const audio_format_t *f)
{
	if (!dst || !src || !f)
		return EINVAL;
	const size_t sample_size = pcm_sample_format_size(f->sample_format);
	if ((size % sample_size) != 0)
		return EINVAL;

	/* This is so ugly it eats kittens, and puppies, and ducklings,
	 * and all little fluffy things...
	 */
#define LOOP_ADD(type, endian, low, high) \
do { \
	const unsigned frame_size = audio_format_frame_size(f); \
	const unsigned frame_count = size / frame_size; \
	for (size_t i = 0; i < frame_count; ++i) { \
		for (unsigned j = 0; j < f->channels; ++j) { \
			const float a = \
			    get_normalized_sample(dst, size, i, j, f);\
			const float b = \
			    get_normalized_sample(src, size, i, j, f);\
			float c = (a + b); \
			if (c < -1.0) c = -1.0; \
			if (c > 1.0) c = 1.0; \
			c += 1.0; \
			c *= ((float)(type)high - (float)(type)low) / 2; \
			c += (float)(type)low; \
			if (c > (float)(type)high) { \
				printf("SCALE HIGH failed\n"); \
			} \
			if (c < (float)(type)low) { \
				printf("SCALE LOW failed\n"); \
			} \
			type *dst_buf = dst; \
			const unsigned pos = i * f->channels  + j; \
			if (pos < (size / sizeof(type))) \
				dst_buf[pos] = to((type)c, type, endian); \
		} \
	} \
} while (0)

	switch (f->sample_format) {
	case PCM_SAMPLE_UINT8:
		LOOP_ADD(uint8_t, le, UINT8_MIN, UINT8_MAX); break;
	case PCM_SAMPLE_SINT8:
		LOOP_ADD(uint8_t, le, INT8_MIN, INT8_MAX); break;
	case PCM_SAMPLE_UINT16_LE:
		LOOP_ADD(uint16_t, le, UINT16_MIN, UINT16_MAX); break;
	case PCM_SAMPLE_SINT16_LE:
		LOOP_ADD(int16_t, le, INT16_MIN, INT16_MAX); break;
	case PCM_SAMPLE_UINT16_BE:
		LOOP_ADD(uint16_t, be, UINT16_MIN, UINT16_MAX); break;
	case PCM_SAMPLE_SINT16_BE:
		LOOP_ADD(int16_t, be, INT16_MIN, INT16_MAX); break;
	case PCM_SAMPLE_UINT24_32_LE:
	case PCM_SAMPLE_UINT32_LE: // TODO this are not right for 24bit
		LOOP_ADD(uint32_t, le, UINT32_MIN, UINT32_MAX); break;
	case PCM_SAMPLE_SINT24_32_LE:
	case PCM_SAMPLE_SINT32_LE:
		LOOP_ADD(int32_t, le, INT32_MIN, INT32_MAX); break;
	case PCM_SAMPLE_UINT24_32_BE:
	case PCM_SAMPLE_UINT32_BE:
		LOOP_ADD(uint32_t, be, UINT32_MIN, UINT32_MAX); break;
	case PCM_SAMPLE_SINT24_32_BE:
	case PCM_SAMPLE_SINT32_BE:
		LOOP_ADD(int32_t, be, INT32_MIN, INT32_MAX); break;
	case PCM_SAMPLE_UINT24_LE:
	case PCM_SAMPLE_SINT24_LE:
	case PCM_SAMPLE_UINT24_BE:
	case PCM_SAMPLE_SINT24_BE:
	case PCM_SAMPLE_FLOAT32:
	default:
		return ENOTSUP;
	}
	return EOK;
#undef LOOP_ADD
}

/** Converts all sample formats to float <-1,1> */
static float get_normalized_sample(const void *buffer, size_t size,
    unsigned frame, unsigned channel, const audio_format_t *f)
{
	assert(f);
	if (channel >= f->channels)
		return 0.0f;
#define GET(type, endian, low, high) \
do { \
	const type *src = buffer; \
	const size_t sample_count = size / sizeof(type); \
	const size_t sample_pos = frame * f->channels + channel; \
	if (sample_pos >= sample_count) {\
		return 0.0f; \
	} \
	float sample = from(src[sample_pos], type, endian); \
	/* This makes it positive */ \
	sample -= (float)(type)low; \
	if (sample < 0.0f) { \
		printf("SUB MIN failed\n"); \
	} \
	/* This makes it <0,2> */ \
	sample /= (((float)(type)high - (float)(type)low) / 2.0f); \
	if (sample > 2.0) { \
		printf("DIV RANGE failed\n"); \
	} \
	return sample - 1.0f; \
} while (0)

	switch (f->sample_format) {
	case PCM_SAMPLE_UINT8:
		GET(uint8_t, le, UINT8_MIN, UINT8_MAX);
	case PCM_SAMPLE_SINT8:
		GET(int8_t, le, INT8_MIN, INT8_MAX);
	case PCM_SAMPLE_UINT16_LE:
		GET(uint16_t, le, UINT16_MIN, UINT16_MAX);
	case PCM_SAMPLE_SINT16_LE:
		GET(int16_t, le, INT16_MIN, INT16_MAX);
	case PCM_SAMPLE_UINT16_BE:
		GET(uint16_t, be, UINT16_MIN, UINT16_MAX);
	case PCM_SAMPLE_SINT16_BE:
		GET(int16_t, be, INT16_MIN, INT16_MAX);
	case PCM_SAMPLE_UINT24_32_LE:
	case PCM_SAMPLE_UINT32_LE:
		GET(uint32_t, le, UINT32_MIN, UINT32_MAX);
	case PCM_SAMPLE_SINT24_32_LE:
	case PCM_SAMPLE_SINT32_LE:
		GET(int32_t, le, INT32_MIN, INT32_MAX);
	case PCM_SAMPLE_UINT24_32_BE:
	case PCM_SAMPLE_UINT32_BE:
		GET(uint32_t, be, UINT32_MIN, UINT32_MAX);
	case PCM_SAMPLE_SINT24_32_BE:
	case PCM_SAMPLE_SINT32_BE:
		GET(int32_t, le, INT32_MIN, INT32_MAX);
	case PCM_SAMPLE_UINT24_LE:
	case PCM_SAMPLE_SINT24_LE:
	case PCM_SAMPLE_UINT24_BE:
	case PCM_SAMPLE_SINT24_BE:
	case PCM_SAMPLE_FLOAT32:
	default: ;
	}
	return 0;
#undef GET
}
/**
 * @}
 */
