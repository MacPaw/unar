/*
 * FILE:	sha2.c
 * AUTHOR:	Aaron D. Gifford
 *		http://www.aarongifford.com/computers/sha.html
 *
 * Copyright (c) 2000-2003, Aaron D. Gifford
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#include <string.h>	/* memcpy()/memset() or bcopy()/bzero() */
#include <assert.h>	/* assert() */
#include "sha.h"

/*
 * ASSERT NOTE:
 * Some sanity checking code is included using assert().  On my FreeBSD
 * system, this additional code can be removed by compiling with NDEBUG
 * defined.  Check your own systems manpage on assert() to see how to
 * compile WITHOUT the sanity checking code on your system.
 *
 * UNROLLED TRANSFORM LOOP NOTE:
 * You can define SHA2_UNROLL_TRANSFORM to use the unrolled transform
 * loop version for the hash transform rounds (defined using macros
 * later in this file).  Either define on the command line, for example:
 *
 *   cc -DSHA2_UNROLL_TRANSFORM -o sha2 sha2.c sha2prog.c
 *
 * or define below:
 *
 *   #define SHA2_UNROLL_TRANSFORM
 *
 */

#if !defined(BYTE_ORDER) || (BYTE_ORDER != LITTLE_ENDIAN && BYTE_ORDER != BIG_ENDIAN)
#include "brg_endian.h"
#define BYTE_ORDER PLATFORM_BYTE_ORDER
#define LITTLE_ENDIAN IS_LITTLE_ENDIAN
#define BIG_ENDIAN IS_BIG_ENDIAN
#endif

/*** ENDIAN REVERSAL MACROS *******************************************/
#if BYTE_ORDER == LITTLE_ENDIAN
#define REVERSE32(w,x)	{ \
	uint32_t tmp = (w); \
	tmp = (tmp >> 16) | (tmp << 16); \
	(x) = ((tmp & 0xff00ff00U) >> 8) | ((tmp & 0x00ff00ffU) << 8); \
}
#define REVERSE64(w,x)	{ \
	uint64_t tmp = (w); \
	tmp = (tmp >> 32) | (tmp << 32); \
	tmp = ((tmp & 0xff00ff00ff00ff00ULL) >> 8) | \
	      ((tmp & 0x00ff00ff00ff00ffULL) << 8); \
	(x) = ((tmp & 0xffff0000ffff0000ULL) >> 16) | \
	      ((tmp & 0x0000ffff0000ffffULL) << 16); \
}
#endif /* BYTE_ORDER == LITTLE_ENDIAN */

/*
 * Macro for incrementally adding the unsigned 64-bit integer n to the
 * unsigned 128-bit integer (represented using a two-element array of
 * 64-bit words):
 */
#define ADDINC128(w,n)	{ \
	(w)[0] += (uint64_t)(n); \
	if ((w)[0] < (n)) { \
		(w)[1]++; \
	} \
}

/*
 * Macros for copying blocks of memory and for zeroing out ranges
 * of memory.  Using these macros makes it easy to switch from
 * using memset()/memcpy() and using bzero()/bcopy().
 *
 * Please define either SHA2_USE_MEMSET_MEMCPY or define
 * SHA2_USE_BZERO_BCOPY depending on which function set you
 * choose to use:
 */
#if !defined(SHA2_USE_MEMSET_MEMCPY) && !defined(SHA2_USE_BZERO_BCOPY)
/* Default to memset()/memcpy() if no option is specified */
#define	SHA2_USE_MEMSET_MEMCPY	1
#endif
#if defined(SHA2_USE_MEMSET_MEMCPY) && defined(SHA2_USE_BZERO_BCOPY)
/* Abort with an error if BOTH options are defined */
#error Define either SHA2_USE_MEMSET_MEMCPY or SHA2_USE_BZERO_BCOPY, not both!
#endif

#ifdef SHA2_USE_MEMSET_MEMCPY
#define MEMSET_BZERO(p,l)	memset((p), 0, (l))
#define MEMCPY_BCOPY(d,s,l)	memcpy((d), (s), (l))
#endif
#ifdef SHA2_USE_BZERO_BCOPY
#define MEMSET_BZERO(p,l)	bzero((p), (l))
#define MEMCPY_BCOPY(d,s,l)	bcopy((s), (d), (l))
#endif


/*** THE SIX LOGICAL FUNCTIONS ****************************************/
/*
 * Bit shifting and rotation (used by the six SHA-XYZ logical functions:
 *
 *   NOTE:  In the original SHA-256/384/512 document, the shift-right
 *   function was named R and the rotate-right function was called S.
 *   (See: http://csrc.nist.gov/cryptval/shs/sha256-384-512.pdf on the
 *   web.)
 *
 *   The newer NIST FIPS 180-2 document uses a much clearer naming
 *   scheme, SHR for shift-right, ROTR for rotate-right, and ROTL for
 *   rotate-left.  (See:
 *   http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 *   on the web.)
 *
 *   WARNING: These macros must be used cautiously, since they reference
 *   supplied parameters sometimes more than once, and thus could have
 *   unexpected side-effects if used without taking this into account.
 */
/* Shift-right (used in SHA-256, SHA-384, and SHA-512): */
#define SHR(b,x) 		((x) >> (b))
/* 32-bit Rotate-right (used in SHA-256): */
#define ROTR32(b,x)	(((x) >> (b)) | ((x) << (32 - (b))))
/* 64-bit Rotate-right (used in SHA-384 and SHA-512): */
#define ROTR64(b,x)	(((x) >> (b)) | ((x) << (64 - (b))))
/* 32-bit Rotate-left (used in SHA-1): */
#define ROTL32(b,x)	(((x) << (b)) | ((x) >> (32 - (b))))

/* Two logical functions used in SHA-1, SHA-254, SHA-256, SHA-384, and SHA-512: */
#define Ch(x,y,z)	(((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x,y,z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/* Function used in SHA-1: */
#define Parity(x,y,z)	((x) ^ (y) ^ (z))

/* Four logical functions used in SHA-256: */
#define Sigma0_256(x)	(ROTR32(2,  (x)) ^ ROTR32(13, (x)) ^ ROTR32(22, (x)))
#define Sigma1_256(x)	(ROTR32(6,  (x)) ^ ROTR32(11, (x)) ^ ROTR32(25, (x)))
#define sigma0_256(x)	(ROTR32(7,  (x)) ^ ROTR32(18, (x)) ^ SHR(   3 , (x)))
#define sigma1_256(x)	(ROTR32(17, (x)) ^ ROTR32(19, (x)) ^ SHR(   10, (x)))

/* Four of six logical functions used in SHA-384 and SHA-512: */
#define Sigma0_512(x)	(ROTR64(28, (x)) ^ ROTR64(34, (x)) ^ ROTR64(39, (x)))
#define Sigma1_512(x)	(ROTR64(14, (x)) ^ ROTR64(18, (x)) ^ ROTR64(41, (x)))
#define sigma0_512(x)	(ROTR64( 1, (x)) ^ ROTR64( 8, (x)) ^ SHR(    7, (x)))
#define sigma1_512(x)	(ROTR64(19, (x)) ^ ROTR64(61, (x)) ^ SHR(    6, (x)))

/*** INTERNAL FUNCTION PROTOTYPES *************************************/

/* SHA-224 and SHA-256: */
static void SHA256_Internal_Init(SHA_CTX*, const uint32_t*);
static void SHA256_Internal_Last(SHA_CTX*);
static void SHA256_Internal_Transform(SHA_CTX*, const uint32_t*);

/* SHA-384 and SHA-512: */
static void SHA512_Internal_Init(SHA_CTX*, const uint64_t*);
static void SHA512_Internal_Last(SHA_CTX*);
static void SHA512_Internal_Transform(SHA_CTX*, const uint64_t*);


/*** SHA2 INITIAL HASH VALUES AND CONSTANTS ***************************/

/* Hash constant words K for SHA-1: */
#define K1_0_TO_19	0x5a827999U
#define K1_20_TO_39	0x6ed9eba1U
#define K1_40_TO_59	0x8f1bbcdcU
#define K1_60_TO_79	0xca62c1d6U

/* Initial hash value H for SHA-1: */
static const uint32_t sha1_initial_hash_value[5] = {
	0x67452301U,
	0xefcdab89U,
	0x98badcfeU,
	0x10325476U,
	0xc3d2e1f0U
};

/* Hash constant words K for SHA-224 and SHA-256: */
static const uint32_t K256[64] = {
	0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
	0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
	0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
	0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
	0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
	0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
	0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
	0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
	0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
	0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
	0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
	0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
	0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
	0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
	0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

/* Initial hash value H for SHA-224: */
static const uint32_t sha224_initial_hash_value[8] = {
	0xc1059ed8U,
	0x367cd507U,
	0x3070dd17U,
	0xf70e5939U,
	0xffc00b31U,
	0x68581511U,
	0x64f98fa7U,
	0xbefa4fa4U
};

/* Initial hash value H for SHA-256: */
static const uint32_t sha256_initial_hash_value[8] = {
	0x6a09e667U,
	0xbb67ae85U,
	0x3c6ef372U,
	0xa54ff53aU,
	0x510e527fU,
	0x9b05688cU,
	0x1f83d9abU,
	0x5be0cd19U
};

/* Hash constant words K for SHA-384 and SHA-512: */
static const uint64_t K512[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
	0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
	0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
	0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
	0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
	0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
	0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
	0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
	0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
	0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
	0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
	0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
	0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
	0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

/* Initial hash value H for SHA-384 */
static const uint64_t sha384_initial_hash_value[8] = {
	0xcbbb9d5dc1059ed8ULL,
	0x629a292a367cd507ULL,
	0x9159015a3070dd17ULL,
	0x152fecd8f70e5939ULL,
	0x67332667ffc00b31ULL,
	0x8eb44a8768581511ULL,
	0xdb0c2e0d64f98fa7ULL,
	0x47b5481dbefa4fa4ULL
};

/* Initial hash value H for SHA-512 */
static const uint64_t sha512_initial_hash_value[8] = {
	0x6a09e667f3bcc908ULL,
	0xbb67ae8584caa73bULL,
	0x3c6ef372fe94f82bULL,
	0xa54ff53a5f1d36f1ULL,
	0x510e527fade682d1ULL,
	0x9b05688c2b3e6c1fULL,
	0x1f83d9abfb41bd6bULL,
	0x5be0cd19137e2179ULL
};

/*
 * Constant used by SHA224/256/384/512_End() functions for converting the
 * digest to a readable hexadecimal character string:
 */
static const char *sha_hex_digits = "0123456789abcdef";


/*** SHA-1: ***********************************************************/
void SHA1_Init(SHA_CTX* context) {
	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	MEMCPY_BCOPY(context->s1.state, sha1_initial_hash_value, sizeof(uint32_t) * 5);
	MEMSET_BZERO(context->s1.buffer, 64);
	context->s1.bitcount = 0;
}

#ifdef SHA2_UNROLL_TRANSFORM

/* Unrolled SHA-1 round macros: */

#if BYTE_ORDER == LITTLE_ENDIAN

#define ROUND1_0_TO_15(a,b,c,d,e)				\
	REVERSE32(*data++, W1[j]);				\
	(e) = ROTL32(5, (a)) + Ch((b), (c), (d)) + (e) +	\
	     K1_0_TO_19 + W1[j];	\
	(b) = ROTL32(30, (b));		\
	j++;

#else /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND1_0_TO_15(a,b,c,d,e)				\
	(e) = ROTL32(5, (a)) + Ch((b), (c), (d)) + (e) +	\
	     K1_0_TO_19 + ( W1[j] = *data++ );		\
	(b) = ROTL32(30, (b));	\
	j++;

#endif /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND1_16_TO_19(a,b,c,d,e)	\
	T1 = W1[(j+13)&0x0f] ^ W1[(j+8)&0x0f] ^ W1[(j+2)&0x0f] ^ W1[j&0x0f];	\
	(e) = ROTL32(5, a) + Ch(b,c,d) + e + K1_0_TO_19 + ( W1[j&0x0f] = ROTL32(1, T1) );	\
	(b) = ROTL32(30, b);	\
	j++;

#define ROUND1_20_TO_39(a,b,c,d,e)	\
	T1 = W1[(j+13)&0x0f] ^ W1[(j+8)&0x0f] ^ W1[(j+2)&0x0f] ^ W1[j&0x0f];	\
	(e) = ROTL32(5, a) + Parity(b,c,d) + e + K1_20_TO_39 + ( W1[j&0x0f] = ROTL32(1, T1) );	\
	(b) = ROTL32(30, b);	\
	j++;

#define ROUND1_40_TO_59(a,b,c,d,e)	\
	T1 = W1[(j+13)&0x0f] ^ W1[(j+8)&0x0f] ^ W1[(j+2)&0x0f] ^ W1[j&0x0f];	\
	(e) = ROTL32(5, a) + Maj(b,c,d) + e + K1_40_TO_59 + ( W1[j&0x0f] = ROTL32(1, T1) );	\
	(b) = ROTL32(30, b);	\
	j++;

#define ROUND1_60_TO_79(a,b,c,d,e)	\
	T1 = W1[(j+13)&0x0f] ^ W1[(j+8)&0x0f] ^ W1[(j+2)&0x0f] ^ W1[j&0x0f];	\
	(e) = ROTL32(5, a) + Parity(b,c,d) + e + K1_60_TO_79 + ( W1[j&0x0f] = ROTL32(1, T1) );	\
	(b) = ROTL32(30, b);	\
	j++;

static void SHA1_Internal_Transform(SHA_CTX* context, const uint32_t* data) {
	uint32_t	a, b, c, d, e;
	uint32_t	T1, *W1;
	int		j;

	W1 = (uint32_t*)context->s1.buffer;

	/* Initialize registers with the prev. intermediate value */
	a = context->s1.state[0];
	b = context->s1.state[1];
	c = context->s1.state[2];
	d = context->s1.state[3];
	e = context->s1.state[4];

	j = 0;

	/* Rounds 0 to 15 unrolled: */
	ROUND1_0_TO_15(a,b,c,d,e);
	ROUND1_0_TO_15(e,a,b,c,d);
	ROUND1_0_TO_15(d,e,a,b,c);
	ROUND1_0_TO_15(c,d,e,a,b);
	ROUND1_0_TO_15(b,c,d,e,a);
	ROUND1_0_TO_15(a,b,c,d,e);
	ROUND1_0_TO_15(e,a,b,c,d);
	ROUND1_0_TO_15(d,e,a,b,c);
	ROUND1_0_TO_15(c,d,e,a,b);
	ROUND1_0_TO_15(b,c,d,e,a);
	ROUND1_0_TO_15(a,b,c,d,e);
	ROUND1_0_TO_15(e,a,b,c,d);
	ROUND1_0_TO_15(d,e,a,b,c);
	ROUND1_0_TO_15(c,d,e,a,b);
	ROUND1_0_TO_15(b,c,d,e,a);
	ROUND1_0_TO_15(a,b,c,d,e);

	/* Rounds 16 to 19 unrolled: */
	ROUND1_16_TO_19(e,a,b,c,d);
	ROUND1_16_TO_19(d,e,a,b,c);
	ROUND1_16_TO_19(c,d,e,a,b);
	ROUND1_16_TO_19(b,c,d,e,a);

	/* Rounds 20 to 39 unrolled: */
	ROUND1_20_TO_39(a,b,c,d,e);
	ROUND1_20_TO_39(e,a,b,c,d);
	ROUND1_20_TO_39(d,e,a,b,c);
	ROUND1_20_TO_39(c,d,e,a,b);
	ROUND1_20_TO_39(b,c,d,e,a);
	ROUND1_20_TO_39(a,b,c,d,e);
	ROUND1_20_TO_39(e,a,b,c,d);
	ROUND1_20_TO_39(d,e,a,b,c);
	ROUND1_20_TO_39(c,d,e,a,b);
	ROUND1_20_TO_39(b,c,d,e,a);
	ROUND1_20_TO_39(a,b,c,d,e);
	ROUND1_20_TO_39(e,a,b,c,d);
	ROUND1_20_TO_39(d,e,a,b,c);
	ROUND1_20_TO_39(c,d,e,a,b);
	ROUND1_20_TO_39(b,c,d,e,a);
	ROUND1_20_TO_39(a,b,c,d,e);
	ROUND1_20_TO_39(e,a,b,c,d);
	ROUND1_20_TO_39(d,e,a,b,c);
	ROUND1_20_TO_39(c,d,e,a,b);
	ROUND1_20_TO_39(b,c,d,e,a);

	/* Rounds 40 to 59 unrolled: */
	ROUND1_40_TO_59(a,b,c,d,e);
	ROUND1_40_TO_59(e,a,b,c,d);
	ROUND1_40_TO_59(d,e,a,b,c);
	ROUND1_40_TO_59(c,d,e,a,b);
	ROUND1_40_TO_59(b,c,d,e,a);
	ROUND1_40_TO_59(a,b,c,d,e);
	ROUND1_40_TO_59(e,a,b,c,d);
	ROUND1_40_TO_59(d,e,a,b,c);
	ROUND1_40_TO_59(c,d,e,a,b);
	ROUND1_40_TO_59(b,c,d,e,a);
	ROUND1_40_TO_59(a,b,c,d,e);
	ROUND1_40_TO_59(e,a,b,c,d);
	ROUND1_40_TO_59(d,e,a,b,c);
	ROUND1_40_TO_59(c,d,e,a,b);
	ROUND1_40_TO_59(b,c,d,e,a);
	ROUND1_40_TO_59(a,b,c,d,e);
	ROUND1_40_TO_59(e,a,b,c,d);
	ROUND1_40_TO_59(d,e,a,b,c);
	ROUND1_40_TO_59(c,d,e,a,b);
	ROUND1_40_TO_59(b,c,d,e,a);

	/* Rounds 60 to 79 unrolled: */
	ROUND1_60_TO_79(a,b,c,d,e);
	ROUND1_60_TO_79(e,a,b,c,d);
	ROUND1_60_TO_79(d,e,a,b,c);
	ROUND1_60_TO_79(c,d,e,a,b);
	ROUND1_60_TO_79(b,c,d,e,a);
	ROUND1_60_TO_79(a,b,c,d,e);
	ROUND1_60_TO_79(e,a,b,c,d);
	ROUND1_60_TO_79(d,e,a,b,c);
	ROUND1_60_TO_79(c,d,e,a,b);
	ROUND1_60_TO_79(b,c,d,e,a);
	ROUND1_60_TO_79(a,b,c,d,e);
	ROUND1_60_TO_79(e,a,b,c,d);
	ROUND1_60_TO_79(d,e,a,b,c);
	ROUND1_60_TO_79(c,d,e,a,b);
	ROUND1_60_TO_79(b,c,d,e,a);
	ROUND1_60_TO_79(a,b,c,d,e);
	ROUND1_60_TO_79(e,a,b,c,d);
	ROUND1_60_TO_79(d,e,a,b,c);
	ROUND1_60_TO_79(c,d,e,a,b);
	ROUND1_60_TO_79(b,c,d,e,a);

	/* Compute the current intermediate hash value */
	context->s1.state[0] += a;
	context->s1.state[1] += b;
	context->s1.state[2] += c;
	context->s1.state[3] += d;
	context->s1.state[4] += e;

	/* Clean up */
	a = b = c = d = e = T1 = 0;
}

#else  /* SHA2_UNROLL_TRANSFORM */

static void SHA1_Internal_Transform(SHA_CTX* context, const uint32_t* data) {
	uint32_t	a, b, c, d, e;
	uint32_t	T1, *W1;
	int		j;

	W1 = (uint32_t*)context->s1.buffer;

	/* Initialize registers with the prev. intermediate value */
	a = context->s1.state[0];
	b = context->s1.state[1];
	c = context->s1.state[2];
	d = context->s1.state[3];
	e = context->s1.state[4];
	j = 0;
	do {
#if BYTE_ORDER == LITTLE_ENDIAN
		/* Copy data while converting to host byte order */
		REVERSE32(*data++, W1[j]);
		T1 = ROTL32(5, a) + Ch(b, c, d) + e + K1_0_TO_19 + W1[j];
#else /* BYTE_ORDER == LITTLE_ENDIAN */
		T1 = ROTL32(5, a) + Ch(b, c, d) + e + K1_0_TO_19 + (W1[j] = *data++);
#endif /* BYTE_ORDER == LITTLE_ENDIAN */
		e = d;
		d = c;
		c = ROTL32(30, b);
		b = a;
		a = T1;
		j++;
	} while (j < 16);

	do {
		T1 = W1[(j+13)&0x0f] ^ W1[(j+8)&0x0f] ^ W1[(j+2)&0x0f] ^ W1[j&0x0f];
		T1 = ROTL32(5, a) + Ch(b,c,d) + e + K1_0_TO_19 + (W1[j&0x0f] = ROTL32(1, T1));
		e = d;
		d = c;
		c = ROTL32(30, b);
		b = a;
		a = T1;
		j++;
	} while (j < 20);

	do {
		T1 = W1[(j+13)&0x0f] ^ W1[(j+8)&0x0f] ^ W1[(j+2)&0x0f] ^ W1[j&0x0f];
		T1 = ROTL32(5, a) + Parity(b,c,d) + e + K1_20_TO_39 + (W1[j&0x0f] = ROTL32(1, T1));
		e = d;
		d = c;
		c = ROTL32(30, b);
		b = a;
		a = T1;
		j++;
	} while (j < 40);

	do {
		T1 = W1[(j+13)&0x0f] ^ W1[(j+8)&0x0f] ^ W1[(j+2)&0x0f] ^ W1[j&0x0f];
		T1 = ROTL32(5, a) + Maj(b,c,d) + e + K1_40_TO_59 + (W1[j&0x0f] = ROTL32(1, T1));
		e = d;
		d = c;
		c = ROTL32(30, b);
		b = a;
		a = T1;
		j++;
	} while (j < 60);

	do {
		T1 = W1[(j+13)&0x0f] ^ W1[(j+8)&0x0f] ^ W1[(j+2)&0x0f] ^ W1[j&0x0f];
		T1 = ROTL32(5, a) + Parity(b,c,d) + e + K1_60_TO_79 + (W1[j&0x0f] = ROTL32(1, T1));
		e = d;
		d = c;
		c = ROTL32(30, b);
		b = a;
		a = T1;
		j++;
	} while (j < 80);


	/* Compute the current intermediate hash value */
	context->s1.state[0] += a;
	context->s1.state[1] += b;
	context->s1.state[2] += c;
	context->s1.state[3] += d;
	context->s1.state[4] += e;

	/* Clean up */
	//a = b = c = d = e = T1 = 0;
}

#endif /* SHA2_UNROLL_TRANSFORM */

void SHA1_Update(SHA_CTX* context, const uint8_t *data, size_t len) {
	size_t freespace, usedspace;
	if (len == 0) {
		/* Calling with no data is valid - we do nothing */
		return;
	}

	/* Sanity check: */
	assert(context != (SHA_CTX*)0 && data != (uint8_t*)0);

	usedspace = (size_t)(context->s1.bitcount >> 3) % 64;
	if (usedspace > 0) {
		/* Calculate how much free space is available in the buffer */
		freespace = 64 - (size_t)usedspace;

		if (len >= freespace) {
			/* Fill the buffer completely and process it */
			MEMCPY_BCOPY(&context->s1.buffer[usedspace], data, freespace);
			context->s1.bitcount += freespace << 3;
			len -= freespace;
			data += freespace;
			SHA1_Internal_Transform(context, (uint32_t*)context->s1.buffer);
		} else {
			/* The buffer is not yet full */
			MEMCPY_BCOPY(&context->s1.buffer[usedspace], data, len);
			context->s1.bitcount += len << 3;
			/* Clean up: */
			//usedspace = freespace = 0;
			return;
		}
	}
	while (len >= 64) {
		/* Process as many complete blocks as we can */
		SHA1_Internal_Transform(context, (uint32_t*)data);
		context->s1.bitcount += 512;
		len -= 64;
		data += 64;
	}
	if (len > 0) {
		/* There's left-overs, so save 'em */
		MEMCPY_BCOPY(context->s1.buffer, data, len);
		context->s1.bitcount += len << 3;
	}
	/* Clean up: */
	//usedspace = freespace = 0;
}

void SHA1_Final(uint8_t digest[], SHA_CTX* context) {
	uint32_t	*d = (uint32_t*)digest;
	size_t	usedspace;

	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	if (digest == (uint8_t*)0) {
		/*
		 * No digest buffer, so we can do nothing
		 * except clean up and go home
		 */
		MEMSET_BZERO(context, sizeof(SHA_CTX));
		return;
	}

	usedspace = (context->s1.bitcount >> 3) % 64;
	if (usedspace == 0) {
		/* Set-up for the last transform: */
		MEMSET_BZERO(context->s1.buffer, 56);

		/* Begin padding with a 1 bit: */
		*context->s1.buffer = 0x80;
	} else {
		/* Begin padding with a 1 bit: */
		context->s1.buffer[usedspace++] = 0x80;

		if (usedspace <= 56) {
			/* Set-up for the last transform: */
			MEMSET_BZERO(&context->s1.buffer[usedspace], 56 - usedspace);
		} else {
			if (usedspace < 64) {
				MEMSET_BZERO(&context->s1.buffer[usedspace], 64 - usedspace);
			}
			/* Do second-to-last transform: */
			SHA1_Internal_Transform(context, (uint32_t*)context->s1.buffer);

			/* And set-up for the last transform: */
			MEMSET_BZERO(context->s1.buffer, 56);
		}
		/* Clean up: */
		//usedspace = 0;
	}
	/* Set the bit count: */
#if BYTE_ORDER == LITTLE_ENDIAN
	/* Convert FROM host byte order */
	REVERSE64(context->s1.bitcount,context->s1.bitcount);
#endif
	*(uint64_t*)&context->s1.buffer[56] = context->s1.bitcount;

	/* Final transform: */
	SHA1_Internal_Transform(context, (uint32_t*)context->s1.buffer);

	/* Save the hash data for output: */
#if BYTE_ORDER == LITTLE_ENDIAN
	{
		/* Convert TO host byte order */
		int	j;
		for (j = 0; j < (SHA1_DIGEST_LENGTH >> 2); j++) {
			REVERSE32(context->s1.state[j],context->s1.state[j]);
			*d++ = context->s1.state[j];
		}
	}
#else
	MEMCPY_BCOPY(d, context->s1.state, SHA1_DIGEST_LENGTH);
#endif

	/* Clean up: */
	MEMSET_BZERO(context, sizeof(SHA_CTX));
}

char *SHA1_End(SHA_CTX* context, char buffer[]) {
	uint8_t	digest[SHA1_DIGEST_LENGTH], *d = digest;
	int		i;

	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	if (buffer != (char*)0) {
		SHA1_Final(digest, context);

		for (i = 0; i < SHA1_DIGEST_LENGTH; i++) {
			*buffer++ = sha_hex_digits[(*d & 0xf0) >> 4];
			*buffer++ = sha_hex_digits[*d & 0x0f];
			d++;
		}
		*buffer = (char)0;
	} else {
		MEMSET_BZERO(context, sizeof(SHA_CTX));
	}
	MEMSET_BZERO(digest, SHA1_DIGEST_LENGTH);
	return buffer;
}

char* SHA1_Data(const uint8_t* data, size_t len, char digest[SHA1_DIGEST_STRING_LENGTH]) {
	SHA_CTX	context;

	SHA1_Init(&context);
	SHA1_Update(&context, data, len);
	return SHA1_End(&context, digest);
}


/*** SHA-256: *********************************************************/
static void SHA256_Internal_Init(SHA_CTX* context, const uint32_t* ihv) {
	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	MEMCPY_BCOPY(context->s256.state, ihv, sizeof(uint32_t) * 8);
	MEMSET_BZERO(context->s256.buffer, 64);
	context->s256.bitcount = 0;
}

void SHA256_Init(SHA_CTX* context) {
	SHA256_Internal_Init(context, sha256_initial_hash_value);
}

#ifdef SHA2_UNROLL_TRANSFORM

/* Unrolled SHA-256 round macros: */

#if BYTE_ORDER == LITTLE_ENDIAN

#define ROUND256_0_TO_15(a,b,c,d,e,f,g,h)	\
	REVERSE32(*data++, W256[j]); \
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + \
             K256[j] + W256[j]; \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++


#else /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND256_0_TO_15(a,b,c,d,e,f,g,h)	\
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + \
	     K256[j] + (W256[j] = *data++); \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++

#endif /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND256(a,b,c,d,e,f,g,h)	\
	s0 = W256[(j+1)&0x0f]; \
	s0 = sigma0_256(s0); \
	s1 = W256[(j+14)&0x0f]; \
	s1 = sigma1_256(s1); \
	T1 = (h) + Sigma1_256(e) + Ch((e), (f), (g)) + K256[j] + \
	     (W256[j&0x0f] += s1 + W256[(j+9)&0x0f] + s0); \
	(d) += T1; \
	(h) = T1 + Sigma0_256(a) + Maj((a), (b), (c)); \
	j++

static void SHA256_Internal_Transform(SHA_CTX* context, const uint32_t* data) {
	uint32_t	a, b, c, d, e, f, g, h, s0, s1;
	uint32_t	T1, *W256;
	int		j;

	W256 = (uint32_t*)context->s256.buffer;

	/* Initialize registers with the prev. intermediate value */
	a = context->s256.state[0];
	b = context->s256.state[1];
	c = context->s256.state[2];
	d = context->s256.state[3];
	e = context->s256.state[4];
	f = context->s256.state[5];
	g = context->s256.state[6];
	h = context->s256.state[7];

	j = 0;
	do {
		/* Rounds 0 to 15 (unrolled): */
		ROUND256_0_TO_15(a,b,c,d,e,f,g,h);
		ROUND256_0_TO_15(h,a,b,c,d,e,f,g);
		ROUND256_0_TO_15(g,h,a,b,c,d,e,f);
		ROUND256_0_TO_15(f,g,h,a,b,c,d,e);
		ROUND256_0_TO_15(e,f,g,h,a,b,c,d);
		ROUND256_0_TO_15(d,e,f,g,h,a,b,c);
		ROUND256_0_TO_15(c,d,e,f,g,h,a,b);
		ROUND256_0_TO_15(b,c,d,e,f,g,h,a);
	} while (j < 16);

	/* Now for the remaining rounds to 64: */
	do {
		ROUND256(a,b,c,d,e,f,g,h);
		ROUND256(h,a,b,c,d,e,f,g);
		ROUND256(g,h,a,b,c,d,e,f);
		ROUND256(f,g,h,a,b,c,d,e);
		ROUND256(e,f,g,h,a,b,c,d);
		ROUND256(d,e,f,g,h,a,b,c);
		ROUND256(c,d,e,f,g,h,a,b);
		ROUND256(b,c,d,e,f,g,h,a);
	} while (j < 64);

	/* Compute the current intermediate hash value */
	context->s256.state[0] += a;
	context->s256.state[1] += b;
	context->s256.state[2] += c;
	context->s256.state[3] += d;
	context->s256.state[4] += e;
	context->s256.state[5] += f;
	context->s256.state[6] += g;
	context->s256.state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = 0;
}

#else /* SHA2_UNROLL_TRANSFORM */

static void SHA256_Internal_Transform(SHA_CTX* context, const uint32_t* data) {
	uint32_t	a, b, c, d, e, f, g, h, s0, s1;
	uint32_t	T1, T2, *W256;
	int		j;

	W256 = (uint32_t*)context->s256.buffer;

	/* Initialize registers with the prev. intermediate value */
	a = context->s256.state[0];
	b = context->s256.state[1];
	c = context->s256.state[2];
	d = context->s256.state[3];
	e = context->s256.state[4];
	f = context->s256.state[5];
	g = context->s256.state[6];
	h = context->s256.state[7];

	j = 0;
	do {
#if BYTE_ORDER == LITTLE_ENDIAN
		/* Copy data while converting to host byte order */
		REVERSE32(*data++,W256[j]);
		/* Apply the SHA-256 compression function to update a..h */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + W256[j];
#else /* BYTE_ORDER == LITTLE_ENDIAN */
		/* Apply the SHA-256 compression function to update a..h with copy */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] + (W256[j] = *data++);
#endif /* BYTE_ORDER == LITTLE_ENDIAN */
		T2 = Sigma0_256(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 16);

	do {
		/* Part of the message block expansion: */
		s0 = W256[(j+1)&0x0f];
		s0 = sigma0_256(s0);
		s1 = W256[(j+14)&0x0f];
		s1 = sigma1_256(s1);

		/* Apply the SHA-256 compression function to update a..h */
		T1 = h + Sigma1_256(e) + Ch(e, f, g) + K256[j] +
		     (W256[j&0x0f] += s1 + W256[(j+9)&0x0f] + s0);
		T2 = Sigma0_256(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 64);

	/* Compute the current intermediate hash value */
	context->s256.state[0] += a;
	context->s256.state[1] += b;
	context->s256.state[2] += c;
	context->s256.state[3] += d;
	context->s256.state[4] += e;
	context->s256.state[5] += f;
	context->s256.state[6] += g;
	context->s256.state[7] += h;

	/* Clean up */
	//a = b = c = d = e = f = g = h = T1 = T2 = 0;
}

#endif /* SHA2_UNROLL_TRANSFORM */

void SHA256_Update(SHA_CTX* context, const uint8_t *data, size_t len) {
	size_t	freespace, usedspace;

	if (len == 0) {
		/* Calling with no data is valid - we do nothing */
		return;
	}

	/* Sanity check: */
	assert(context != (SHA_CTX*)0 && data != (uint8_t*)0);

	usedspace = (size_t)(context->s256.bitcount >> 3) % 64;
	if (usedspace > 0) {
		/* Calculate how much free space is available in the buffer */
		freespace = 64 - usedspace;

		if (len >= freespace) {
			/* Fill the buffer completely and process it */
			MEMCPY_BCOPY(&context->s256.buffer[usedspace], data, freespace);
			context->s256.bitcount += freespace << 3;
			len -= freespace;
			data += freespace;
			SHA256_Internal_Transform(context, (uint32_t*)context->s256.buffer);
		} else {
			/* The buffer is not yet full */
			MEMCPY_BCOPY(&context->s256.buffer[usedspace], data, len);
			context->s256.bitcount += len << 3;
			/* Clean up: */
			//usedspace = freespace = 0;
			return;
		}
	}
	while (len >= 64) {
		/* Process as many complete blocks as we can */
		SHA256_Internal_Transform(context, (uint32_t*)data);
		context->s256.bitcount += 512;
		len -= 64;
		data += 64;
	}
	if (len > 0) {
		/* There's left-overs, so save 'em */
		MEMCPY_BCOPY(context->s256.buffer, data, len);
		context->s256.bitcount += len << 3;
	}
	/* Clean up: */
	//usedspace = freespace = 0;
}

static void SHA256_Internal_Last(SHA_CTX* context) {
	size_t	usedspace;

	usedspace = (size_t)(context->s256.bitcount >> 3) % 64;
#if BYTE_ORDER == LITTLE_ENDIAN
	/* Convert FROM host byte order */
	REVERSE64(context->s256.bitcount,context->s256.bitcount);
#endif
	if (usedspace > 0) {
		/* Begin padding with a 1 bit: */
		context->s256.buffer[usedspace++] = 0x80;

		if (usedspace <= 56) {
			/* Set-up for the last transform: */
			MEMSET_BZERO(&context->s256.buffer[usedspace], 56 - usedspace);
		} else {
			if (usedspace < 64) {
				MEMSET_BZERO(&context->s256.buffer[usedspace], 64 - usedspace);
			}
			/* Do second-to-last transform: */
			SHA256_Internal_Transform(context, (uint32_t*)context->s256.buffer);

			/* And set-up for the last transform: */
			MEMSET_BZERO(context->s256.buffer, 56);
		}
		/* Clean up: */
		//usedspace = 0;
	} else {
		/* Set-up for the last transform: */
		MEMSET_BZERO(context->s256.buffer, 56);

		/* Begin padding with a 1 bit: */
		*context->s256.buffer = 0x80;
	}
	/* Set the bit count: */
	*(uint64_t*)&context->s256.buffer[56] = context->s256.bitcount;

	/* Final transform: */
	SHA256_Internal_Transform(context, (uint32_t*)context->s256.buffer);
}

void SHA256_Final(uint8_t digest[], SHA_CTX* context) {
	uint32_t	*d = (uint32_t*)digest;

	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (uint8_t*)0) {
		SHA256_Internal_Last(context);

		/* Save the hash data for output: */
#if BYTE_ORDER == LITTLE_ENDIAN
		{
			/* Convert TO host byte order */
			int	j;
			for (j = 0; j < (SHA256_DIGEST_LENGTH >> 2); j++) {
				REVERSE32(context->s256.state[j],context->s256.state[j]);
				*d++ = context->s256.state[j];
			}
		}
#else
		MEMCPY_BCOPY(d, context->s256.state, SHA256_DIGEST_LENGTH);
#endif
	}

	/* Clean up state data: */
	MEMSET_BZERO(context, sizeof(SHA_CTX));
}

char *SHA256_End(SHA_CTX* context, char buffer[]) {
	uint8_t	digest[SHA256_DIGEST_LENGTH], *d = digest;
	int		i;

	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	if (buffer != (char*)0) {
		SHA256_Final(digest, context);

		for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
			*buffer++ = sha_hex_digits[(*d & 0xf0) >> 4];
			*buffer++ = sha_hex_digits[*d & 0x0f];
			d++;
		}
		*buffer = (char)0;
	} else {
		MEMSET_BZERO(context, sizeof(SHA_CTX));
	}
	MEMSET_BZERO(digest, SHA256_DIGEST_LENGTH);
	return buffer;
}

char* SHA256_Data(const uint8_t* data, size_t len, char digest[SHA256_DIGEST_STRING_LENGTH]) {
	SHA_CTX	context;

	SHA256_Init(&context);
	SHA256_Update(&context, data, len);
	return SHA256_End(&context, digest);
}


/*** SHA-224: *********************************************************/
void SHA224_Init(SHA_CTX* context) {
	SHA256_Internal_Init(context, sha224_initial_hash_value);
}

void SHA224_Update(SHA_CTX* context, const uint8_t *data, size_t len) {
	SHA256_Update(context, data, len);
}

void SHA224_Final(uint8_t digest[], SHA_CTX* context) {
	uint32_t	*d = (uint32_t*)digest;

	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (uint8_t*)0) {
		SHA256_Internal_Last(context);

		/* Save the hash data for output: */
#if BYTE_ORDER == LITTLE_ENDIAN
		{
			/* Convert TO host byte order */
			int	j;
			for (j = 0; j < (SHA224_DIGEST_LENGTH >> 2); j++) {
				REVERSE32(context->s256.state[j],context->s256.state[j]);
				*d++ = context->s256.state[j];
			}
		}
#else
		MEMCPY_BCOPY(d, context->s256.state, SHA224_DIGEST_LENGTH);
#endif
	}

	/* Clean up state data: */
	MEMSET_BZERO(context, sizeof(SHA_CTX));
}

char *SHA224_End(SHA_CTX* context, char buffer[]) {
	uint8_t	digest[SHA224_DIGEST_LENGTH], *d = digest;
	int		i;

	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	if (buffer != (char*)0) {
		SHA224_Final(digest, context);

		for (i = 0; i < SHA224_DIGEST_LENGTH; i++) {
			*buffer++ = sha_hex_digits[(*d & 0xf0) >> 4];
			*buffer++ = sha_hex_digits[*d & 0x0f];
			d++;
		}
		*buffer = (char)0;
	} else {
		MEMSET_BZERO(context, sizeof(SHA_CTX));
	}
	MEMSET_BZERO(digest, SHA224_DIGEST_LENGTH);
	return buffer;
}

char* SHA224_Data(const uint8_t* data, size_t len, char digest[SHA224_DIGEST_STRING_LENGTH]) {
	SHA_CTX	context;

	SHA224_Init(&context);
	SHA224_Update(&context, data, len);
	return SHA224_End(&context, digest);
}


/*** SHA-512: *********************************************************/
static void SHA512_Internal_Init(SHA_CTX* context, const uint64_t* ihv) {
	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	MEMCPY_BCOPY(context->s512.state, ihv, sizeof(uint64_t) * 8);
	MEMSET_BZERO(context->s512.buffer, 128);
	context->s512.bitcount[0] = context->s512.bitcount[1] =  0;
}

void SHA512_Init(SHA_CTX* context) {
	SHA512_Internal_Init(context, sha512_initial_hash_value);
}

#ifdef SHA2_UNROLL_TRANSFORM

/* Unrolled SHA-512 round macros: */
#if BYTE_ORDER == LITTLE_ENDIAN

#define ROUND512_0_TO_15(a,b,c,d,e,f,g,h)	\
	REVERSE64(*data++, W512[j]); \
	T1 = (h) + Sigma1_512(e) + Ch((e), (f), (g)) + \
             K512[j] + W512[j]; \
	(d) += T1, \
	(h) = T1 + Sigma0_512(a) + Maj((a), (b), (c)), \
	j++


#else /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND512_0_TO_15(a,b,c,d,e,f,g,h)	\
	T1 = (h) + Sigma1_512(e) + Ch((e), (f), (g)) + \
             K512[j] + (W512[j] = *data++); \
	(d) += T1; \
	(h) = T1 + Sigma0_512(a) + Maj((a), (b), (c)); \
	j++

#endif /* BYTE_ORDER == LITTLE_ENDIAN */

#define ROUND512(a,b,c,d,e,f,g,h)	\
	s0 = W512[(j+1)&0x0f]; \
	s0 = sigma0_512(s0); \
	s1 = W512[(j+14)&0x0f]; \
	s1 = sigma1_512(s1); \
	T1 = (h) + Sigma1_512(e) + Ch((e), (f), (g)) + K512[j] + \
             (W512[j&0x0f] += s1 + W512[(j+9)&0x0f] + s0); \
	(d) += T1; \
	(h) = T1 + Sigma0_512(a) + Maj((a), (b), (c)); \
	j++

static void SHA512_Internal_Transform(SHA_CTX* context, const uint64_t* data) {
	uint64_t	a, b, c, d, e, f, g, h, s0, s1;
	uint64_t	T1, *W512 = (uint64_t*)context->s512.buffer;
	int		j;

	/* Initialize registers with the prev. intermediate value */
	a = context->s512.state[0];
	b = context->s512.state[1];
	c = context->s512.state[2];
	d = context->s512.state[3];
	e = context->s512.state[4];
	f = context->s512.state[5];
	g = context->s512.state[6];
	h = context->s512.state[7];

	j = 0;
	do {
		ROUND512_0_TO_15(a,b,c,d,e,f,g,h);
		ROUND512_0_TO_15(h,a,b,c,d,e,f,g);
		ROUND512_0_TO_15(g,h,a,b,c,d,e,f);
		ROUND512_0_TO_15(f,g,h,a,b,c,d,e);
		ROUND512_0_TO_15(e,f,g,h,a,b,c,d);
		ROUND512_0_TO_15(d,e,f,g,h,a,b,c);
		ROUND512_0_TO_15(c,d,e,f,g,h,a,b);
		ROUND512_0_TO_15(b,c,d,e,f,g,h,a);
	} while (j < 16);

	/* Now for the remaining rounds up to 79: */
	do {
		ROUND512(a,b,c,d,e,f,g,h);
		ROUND512(h,a,b,c,d,e,f,g);
		ROUND512(g,h,a,b,c,d,e,f);
		ROUND512(f,g,h,a,b,c,d,e);
		ROUND512(e,f,g,h,a,b,c,d);
		ROUND512(d,e,f,g,h,a,b,c);
		ROUND512(c,d,e,f,g,h,a,b);
		ROUND512(b,c,d,e,f,g,h,a);
	} while (j < 80);

	/* Compute the current intermediate hash value */
	context->s512.state[0] += a;
	context->s512.state[1] += b;
	context->s512.state[2] += c;
	context->s512.state[3] += d;
	context->s512.state[4] += e;
	context->s512.state[5] += f;
	context->s512.state[6] += g;
	context->s512.state[7] += h;

	/* Clean up */
	a = b = c = d = e = f = g = h = T1 = 0;
}

#else /* SHA2_UNROLL_TRANSFORM */

static void SHA512_Internal_Transform(SHA_CTX* context, const uint64_t* data) {
	uint64_t	a, b, c, d, e, f, g, h, s0, s1;
	uint64_t	T1, T2, *W512 = (uint64_t*)context->s512.buffer;
	int		j;

	/* Initialize registers with the prev. intermediate value */
	a = context->s512.state[0];
	b = context->s512.state[1];
	c = context->s512.state[2];
	d = context->s512.state[3];
	e = context->s512.state[4];
	f = context->s512.state[5];
	g = context->s512.state[6];
	h = context->s512.state[7];

	j = 0;
	do {
#if BYTE_ORDER == LITTLE_ENDIAN
		/* Convert TO host byte order */
		REVERSE64(*data++, W512[j]);
		/* Apply the SHA-512 compression function to update a..h */
		T1 = h + Sigma1_512(e) + Ch(e, f, g) + K512[j] + W512[j];
#else /* BYTE_ORDER == LITTLE_ENDIAN */
		/* Apply the SHA-512 compression function to update a..h with copy */
		T1 = h + Sigma1_512(e) + Ch(e, f, g) + K512[j] + (W512[j] = *data++);
#endif /* BYTE_ORDER == LITTLE_ENDIAN */
		T2 = Sigma0_512(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 16);

	do {
		/* Part of the message block expansion: */
		s0 = W512[(j+1)&0x0f];
		s0 = sigma0_512(s0);
		s1 = W512[(j+14)&0x0f];
		s1 =  sigma1_512(s1);

		/* Apply the SHA-512 compression function to update a..h */
		T1 = h + Sigma1_512(e) + Ch(e, f, g) + K512[j] +
		     (W512[j&0x0f] += s1 + W512[(j+9)&0x0f] + s0);
		T2 = Sigma0_512(a) + Maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;

		j++;
	} while (j < 80);

	/* Compute the current intermediate hash value */
	context->s512.state[0] += a;
	context->s512.state[1] += b;
	context->s512.state[2] += c;
	context->s512.state[3] += d;
	context->s512.state[4] += e;
	context->s512.state[5] += f;
	context->s512.state[6] += g;
	context->s512.state[7] += h;

	/* Clean up */
	//a = b = c = d = e = f = g = h = T1 = T2 = 0;
}

#endif /* SHA2_UNROLL_TRANSFORM */

void SHA512_Update(SHA_CTX* context, const uint8_t *data, size_t len) {
	size_t	freespace, usedspace;

	if (len == 0) {
		/* Calling with no data is valid - we do nothing */
		return;
	}

	/* Sanity check: */
	assert(context != (SHA_CTX*)0 && data != (uint8_t*)0);

	usedspace = (size_t)(context->s512.bitcount[0] >> 3) % 128;
	if (usedspace > 0) {
		/* Calculate how much free space is available in the buffer */
		freespace = 128 - usedspace;

		if (len >= freespace) {
			/* Fill the buffer completely and process it */
			MEMCPY_BCOPY(&context->s512.buffer[usedspace], data, freespace);
			ADDINC128(context->s512.bitcount, freespace << 3);
			len -= freespace;
			data += freespace;
			SHA512_Internal_Transform(context, (uint64_t*)context->s512.buffer);
		} else {
			/* The buffer is not yet full */
			MEMCPY_BCOPY(&context->s512.buffer[usedspace], data, len);
			ADDINC128(context->s512.bitcount, len << 3);
			/* Clean up: */
			//usedspace = freespace = 0;
			return;
		}
	}
	while (len >= 128) {
		/* Process as many complete blocks as we can */
		SHA512_Internal_Transform(context, (uint64_t*)data);
		ADDINC128(context->s512.bitcount, 1024);
		len -= 128;
		data += 128;
	}
	if (len > 0) {
		/* There's left-overs, so save 'em */
		MEMCPY_BCOPY(context->s512.buffer, data, len);
		ADDINC128(context->s512.bitcount, len << 3);
	}
	/* Clean up: */
	//usedspace = freespace = 0;
}

static void SHA512_Internal_Last(SHA_CTX* context) {
	size_t	usedspace;

	usedspace = (size_t)(context->s512.bitcount[0] >> 3) % 128;
#if BYTE_ORDER == LITTLE_ENDIAN
	/* Convert FROM host byte order */
	REVERSE64(context->s512.bitcount[0],context->s512.bitcount[0]);
	REVERSE64(context->s512.bitcount[1],context->s512.bitcount[1]);
#endif
	if (usedspace > 0) {
		/* Begin padding with a 1 bit: */
		context->s512.buffer[usedspace++] = 0x80;

		if (usedspace <= 112) {
			/* Set-up for the last transform: */
			MEMSET_BZERO(&context->s512.buffer[usedspace], 112 - usedspace);
		} else {
			if (usedspace < 128) {
				MEMSET_BZERO(&context->s512.buffer[usedspace], 128 - usedspace);
			}
			/* Do second-to-last transform: */
			SHA512_Internal_Transform(context, (uint64_t*)context->s512.buffer);

			/* And set-up for the last transform: */
			MEMSET_BZERO(context->s512.buffer, 112);
		}
		/* Clean up: */
		//usedspace = 0;
	} else {
		/* Prepare for final transform: */
		MEMSET_BZERO(context->s512.buffer, 112);

		/* Begin padding with a 1 bit: */
		*context->s512.buffer = 0x80;
	}
	/* Store the length of input data (in bits): */
	*(uint64_t*)&context->s512.buffer[112] = context->s512.bitcount[1];
	*(uint64_t*)&context->s512.buffer[120] = context->s512.bitcount[0];

	/* Final transform: */
	SHA512_Internal_Transform(context, (uint64_t*)context->s512.buffer);
}

void SHA512_Final(uint8_t digest[], SHA_CTX* context) {
	uint64_t	*d = (uint64_t*)digest;

	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (uint8_t*)0) {
		SHA512_Internal_Last(context);

		/* Save the hash data for output: */
#if BYTE_ORDER == LITTLE_ENDIAN
		{
			/* Convert TO host byte order */
			int	j;
			for (j = 0; j < (SHA512_DIGEST_LENGTH >> 3); j++) {
				REVERSE64(context->s512.state[j],context->s512.state[j]);
				*d++ = context->s512.state[j];
			}
		}
#else
		MEMCPY_BCOPY(d, context->s512.state, SHA512_DIGEST_LENGTH);
#endif
	}

	/* Zero out state data */
	MEMSET_BZERO(context, sizeof(SHA_CTX));
}

char *SHA512_End(SHA_CTX* context, char buffer[]) {
	uint8_t	digest[SHA512_DIGEST_LENGTH], *d = digest;
	int		i;

	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	if (buffer != (char*)0) {
		SHA512_Final(digest, context);

		for (i = 0; i < SHA512_DIGEST_LENGTH; i++) {
			*buffer++ = sha_hex_digits[(*d & 0xf0) >> 4];
			*buffer++ = sha_hex_digits[*d & 0x0f];
			d++;
		}
		*buffer = (char)0;
	} else {
		MEMSET_BZERO(context, sizeof(SHA_CTX));
	}
	MEMSET_BZERO(digest, SHA512_DIGEST_LENGTH);
	return buffer;
}

char* SHA512_Data(const uint8_t* data, size_t len, char digest[SHA512_DIGEST_STRING_LENGTH]) {
	SHA_CTX	context;

	SHA512_Init(&context);
	SHA512_Update(&context, data, len);
	return SHA512_End(&context, digest);
}


/*** SHA-384: *********************************************************/
void SHA384_Init(SHA_CTX* context) {
	SHA512_Internal_Init(context, sha384_initial_hash_value);
}

void SHA384_Update(SHA_CTX* context, const uint8_t* data, size_t len) {
	SHA512_Update(context, data, len);
}

void SHA384_Final(uint8_t digest[], SHA_CTX* context) {
	uint64_t	*d = (uint64_t*)digest;

	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	/* If no digest buffer is passed, we don't bother doing this: */
	if (digest != (uint8_t*)0) {
		SHA512_Internal_Last(context);

		/* Save the hash data for output: */
#if BYTE_ORDER == LITTLE_ENDIAN
		{
			/* Convert TO host byte order */
			int	j;
			for (j = 0; j < (SHA384_DIGEST_LENGTH >> 3); j++) {
				REVERSE64(context->s512.state[j],context->s512.state[j]);
				*d++ = context->s512.state[j];
			}
		}
#else
		MEMCPY_BCOPY(d, context->s512.state, SHA384_DIGEST_LENGTH);
#endif
	}

	/* Zero out state data */
	MEMSET_BZERO(context, sizeof(SHA_CTX));
}

char *SHA384_End(SHA_CTX* context, char buffer[]) {
	uint8_t	digest[SHA384_DIGEST_LENGTH], *d = digest;
	int		i;

	/* Sanity check: */
	assert(context != (SHA_CTX*)0);

	if (buffer != (char*)0) {
		SHA384_Final(digest, context);

		for (i = 0; i < SHA384_DIGEST_LENGTH; i++) {
			*buffer++ = sha_hex_digits[(*d & 0xf0) >> 4];
			*buffer++ = sha_hex_digits[*d & 0x0f];
			d++;
		}
		*buffer = (char)0;
	} else {
		MEMSET_BZERO(context, sizeof(SHA_CTX));
	}
	MEMSET_BZERO(digest, SHA384_DIGEST_LENGTH);
	return buffer;
}

char *SHA384_Data(const uint8_t* data, size_t len, char digest[SHA384_DIGEST_STRING_LENGTH]) {
	SHA_CTX	context;

	SHA384_Init(&context);
	SHA384_Update(&context, data, len);
	return SHA384_End(&context, digest);
}
