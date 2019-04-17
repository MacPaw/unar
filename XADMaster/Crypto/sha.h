/*
 * FILE:    sha2.h
 * AUTHOR:  Aaron D. Gifford
 *          http://www.aarongifford.com/computers/sha.html
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

#ifndef __SHA2_H__
#define __SHA2_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <stdint.h>


/*** SHA-224/256/384/512 Various Length Definitions *******************/

/* Digest lengths for SHA-1/224/256/384/512 */
#define   SHA1_DIGEST_LENGTH          20
#define   SHA1_DIGEST_STRING_LENGTH  (SHA1_DIGEST_LENGTH   * 2 + 1)
#define SHA224_DIGEST_LENGTH          28
#define SHA224_DIGEST_STRING_LENGTH  (SHA224_DIGEST_LENGTH * 2 + 1)
#define SHA256_DIGEST_LENGTH          32
#define SHA256_DIGEST_STRING_LENGTH  (SHA256_DIGEST_LENGTH * 2 + 1)
#define SHA384_DIGEST_LENGTH          48
#define SHA384_DIGEST_STRING_LENGTH  (SHA384_DIGEST_LENGTH * 2 + 1)
#define SHA512_DIGEST_LENGTH          64
#define SHA512_DIGEST_STRING_LENGTH  (SHA512_DIGEST_LENGTH * 2 + 1)


typedef union _SHA_CTX {
    /* SHA-1 uses this part of the union: */
    struct {
        uint32_t state[5];
        uint64_t bitcount;
        uint8_t  buffer[64];
    } s1;

    /* SHA-224 and SHA-256 use this part of the union: */
    struct {
        uint32_t state[8];
        uint64_t bitcount;
        uint8_t  buffer[64];
    } s256;

    /* SHA-384 and SHA-512 use this part of the union: */
    struct {
        uint64_t state[8];
        uint64_t bitcount[2];
        uint8_t  buffer[128];
    } s512;
} SHA_CTX;


/*** SHA-256/384/512 Function Prototypes ******************************/

void SHA1_Init(SHA_CTX*);
void SHA1_Update(SHA_CTX*, const uint8_t*, size_t);
void SHA1_Final(uint8_t[SHA1_DIGEST_LENGTH], SHA_CTX*);
char* SHA1_End(SHA_CTX*, char[SHA1_DIGEST_STRING_LENGTH]);
char* SHA1_Data(const uint8_t*, size_t, char[SHA1_DIGEST_STRING_LENGTH]);

void SHA224_Init(SHA_CTX*);
void SHA224_Update(SHA_CTX*, const uint8_t*, size_t);
void SHA224_Final(uint8_t[SHA224_DIGEST_LENGTH], SHA_CTX*);
char* SHA224_End(SHA_CTX*, char[SHA224_DIGEST_STRING_LENGTH]);
char* SHA224_Data(const uint8_t*, size_t, char[SHA224_DIGEST_STRING_LENGTH]);

void SHA256_Init(SHA_CTX*);
void SHA256_Update(SHA_CTX*, const uint8_t*, size_t);
void SHA256_Final(uint8_t[SHA256_DIGEST_LENGTH], SHA_CTX*);
char* SHA256_End(SHA_CTX*, char[SHA256_DIGEST_STRING_LENGTH]);
char* SHA256_Data(const uint8_t*, size_t, char[SHA256_DIGEST_STRING_LENGTH]);

void SHA384_Init(SHA_CTX*);
void SHA384_Update(SHA_CTX*, const uint8_t*, size_t);
void SHA384_Final(uint8_t[SHA384_DIGEST_LENGTH], SHA_CTX*);
char* SHA384_End(SHA_CTX*, char[SHA384_DIGEST_STRING_LENGTH]);
char* SHA384_Data(const uint8_t*, size_t, char[SHA384_DIGEST_STRING_LENGTH]);

void SHA512_Init(SHA_CTX*);
void SHA512_Update(SHA_CTX*, const uint8_t*, size_t);
void SHA512_Final(uint8_t[SHA512_DIGEST_LENGTH], SHA_CTX*);
char* SHA512_End(SHA_CTX*, char[SHA512_DIGEST_STRING_LENGTH]);
char* SHA512_Data(const uint8_t*, size_t, char[SHA512_DIGEST_STRING_LENGTH]);

#ifdef    __cplusplus
}
#endif /* __cplusplus */

#endif /* __SHA2_H__ */

