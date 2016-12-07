#include <stdio.h>
#include <errno.h>
#include <openssl/bio.h>
#include <assert.h>
#include <memory.h>

#define BIO_TYPE_DWRAP       (50 | 0x0400 | 0x0200)

static int dwrap_new(BIO *bio);
static int dwrap_free(BIO *a);
static int dwrap_read(BIO *b, char *out, int outl);
static int dwrap_write(BIO *b, const char *in, int inl);
static int dwrap_puts(BIO *b, const char *in);
static int dwrap_gets(BIO *b, char *buf, int size);
static long dwrap_ctrl(BIO *b, int cmd, long num, void *ptr);  // NOLINT(runtime/int)
static long dwrap_callback_ctrl(BIO *b, int cmd, bio_info_cb *fp);  // NOLINT(runtime/int)

static BIO_METHOD methods_dwrap = {
  BIO_TYPE_DWRAP,
  "dtls_wrapper",
  dwrap_write,
  dwrap_read,
  dwrap_puts,
  dwrap_gets,
  dwrap_ctrl,
  dwrap_new,
  dwrap_free,
  dwrap_callback_ctrl
};

typedef struct BIO_F_DWRAP_CTX_ {
  int dgram_timer_exp;
} BIO_F_DWRAP_CTX;


BIO_METHOD *BIO_f_dwrap(void) {
  return(&methods_dwrap);
}

static int dwrap_new(BIO *bi) {
  BIO_F_DWRAP_CTX *ctx = OPENSSL_malloc(sizeof(BIO_F_BUFFER_CTX));
  if (!ctx) return(0);

  memset(ctx, 0, sizeof(BIO_F_BUFFER_CTX));

  bi->init = 1;
  bi->ptr = (char *)ctx;  // NOLINT
  bi->flags = 0;

  return 1;
}

static int dwrap_free(BIO *a) {
  if (a == NULL) return 0;

  OPENSSL_free(a->ptr);
  a->ptr = NULL;
  a->init = 0;
  a->flags = 0;
  return 1;
}

static int dwrap_read(BIO *b, char *out, int outl) {
  int ret;
  if (!b || !out) {
    return 0;
  }

  BIO_clear_retry_flags(b);

  ret = BIO_read(b->next_bio, out, outl);

  if (ret <= 0) {
    BIO_copy_next_retry(b);
  }

  return ret;
}

static int dwrap_write(BIO *b, const char *in, int inl) {
  if (!b || !in || (inl <= 0)) {
    return 0;
  }

  int ret = BIO_write(b->next_bio, in, inl);
  return ret;
}

static int dwrap_puts(BIO *b, const char *in) {
  assert(0);

  return 0;
}

static int dwrap_gets(BIO *b, char *buf, int size) {
  assert(0);

  return 0;
}

static long dwrap_ctrl(BIO *b, int cmd, long num, void *ptr) {  // NOLINT(runtime/int)
  long ret;  // NOLINT(runtime/int)
  BIO_F_DWRAP_CTX *ctx;

  ctx = b->ptr;

  switch (cmd) {
    case BIO_CTRL_DGRAM_GET_RECV_TIMER_EXP:
      if (ctx->dgram_timer_exp) {
        ret = 1;
        ctx->dgram_timer_exp = 0;
      } else {
        ret = 0;
      }
      break;

    /* Overloading this */
    case BIO_CTRL_DGRAM_SET_RECV_TIMEOUT:
      ctx->dgram_timer_exp = 1;
      ret = 1;
      break;
    default:
      ret = BIO_ctrl(b->next_bio, cmd, num, ptr);
      break;
  }

  return ret;
}

static long dwrap_callback_ctrl(BIO *b, int cmd, bio_info_cb *fp) {  // NOLINT(runtime/int)
  long ret;  // NOLINT(runtime/int)

  ret = BIO_callback_ctrl(b->next_bio, cmd, fp);

  return ret;
}


/* ====================================================================

Copyright (c) 2007-2008, Eric Rescorla and Derek MacDonald
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. None of the contributors names may be used to endorse or promote
products derived from this software without specific prior written
permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

==================================================================== */
