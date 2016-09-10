/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*
This file compares just the decode time of the different binary symbol coders
 in av1.
A random set of 100M binary symbols is drawn from a random set of contexts
 and then encoded and decoded a number of times with each of the entropy coder
 backends.
The average decode time is then printed out and for daala and ans, the
 percentage of decode time as compared to vpxbool is displayed.

To compile this file, use the following commands:

./configure --enable-experimental --enable-daala_ec
clang -O2 -fno-omit-frame-pointer -I. bintest.c aom_dsp/daalaboolwriter.c \
 aom_dsp/daalaboolreader.c aom_dsp/dkboolwriter.c aom_dsp/dkboolreader.c \
 aom_dsp/prob.c aom_dsp/entdec.c aom_dsp/entenc.c aom_dsp/entcode.c \
 av1/common/odintrin.c -o bintest
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <locale.h>
#include "aom_dsp/ansreader.h"
#include "aom_dsp/answriter.h"
#include "aom_dsp/daalaboolreader.h"
#include "aom_dsp/daalaboolwriter.h"
#include "aom_dsp/dkboolreader.h"
#include "aom_dsp/dkboolwriter.h"

#define BINS 100000000
#define TIMES 5

unsigned char buf[BINS/8];
int p[BINS];
int s[BINS];
int b[BINS];

int rand_int(int limit) {
  int div;
  int ret;
  div = RAND_MAX/limit;
  do {
    ret = rand() / div;
  }
  while (ret == limit);

  return ret;
}

static void gen() {
  int i;
  setlocale(LC_NUMERIC, "");
  fprintf(stdout, "Generating %'d binary symbols... ", BINS);
  fflush(stdout);
  srand(0);
  for (i = 0; i < BINS; i++) {
    p[i] = rand_int(255) + 1;
    s[i] = rand_int(256) >= p[i];
  }
  fprintf(stdout, "done!\n");
}

static void test() {
  int i;
  for (i = 0; i < BINS; i++) {
    if (b[i] != s[i]) {
      printf("Error, i = %i, p = %i, s = %i, b = %i\n", i, p[i], s[i], b[i]);
    }
  }
}

static int daala_enc() {
  int i;
  daala_writer dw;
  aom_daala_start_encode(&dw, buf);
  for (i = 0; i < BINS; i++) {
    aom_daala_write(&dw, s[i], p[i]);
  }
  aom_daala_stop_encode(&dw);
  return dw.pos;
}

static double daala_dec(int pos) {
  int i;
  daala_reader dr;
  struct timeval t0, t1;
  gettimeofday(&t0, 0);
  aom_daala_reader_init(&dr, buf, pos);
  for (i = 0; i < BINS; i++) {
    b[i] = aom_daala_read(&dr, p[i]);
  }
  gettimeofday(&t1, 0);
  test();
  return (t1.tv_sec - t0.tv_sec)*1000.0f + (t1.tv_usec - t0.tv_usec)/1000.0f;
}

static int vpx_enc() {
  int i;
  aom_dk_writer dw;
  aom_dk_start_encode(&dw, buf);
  for (i = 0; i < BINS; i++) {
    aom_dk_write(&dw, s[i], p[i]);
  }
  aom_dk_stop_encode(&dw);
  return dw.pos;
}

static double vpx_dec(int pos) {
  int i;
  struct aom_dk_reader dr;
  struct timeval t0, t1;
  gettimeofday(&t0, 0);
  aom_dk_reader_init(&dr, buf, pos, NULL, NULL);
  for (i = 0; i < BINS; i++) {
    b[i] = aom_dk_read(&dr, p[i]);
  }
  gettimeofday(&t1, 0);
  test();
  return (t1.tv_sec - t0.tv_sec)*1000.0f + (t1.tv_usec - t0.tv_usec)/1000.0f;
}

static int ans_enc() {
  int i;
  struct AnsCoder ac;
  ans_write_init(&ac, buf);
  /* The uabs and rans encoders reqiure the symbols to be encode in reverse
      order. */
  for (i = BINS; i-- > 0; ) {
    uabs_write(&ac, s[i], p[i]);
  }
  return ans_write_end(&ac);
}

static double ans_dec(int pos) {
  int i;
  struct AnsDecoder ad;
  struct timeval t0, t1;
  gettimeofday(&t0, 0);
  ans_read_init(&ad, buf, pos);
  for (i = 0; i < BINS; i++) {
    b[i] = uabs_read(&ad, p[i]);
  }
  gettimeofday(&t1, 0);
  test();
  return (t1.tv_sec - t0.tv_sec)*1000.0f + (t1.tv_usec - t0.tv_usec)/1000.0f;
}

int main(int argc, char *argv[]) {
  int b;
  int i;
  double daala;
  double vpx;
  double ans;
  gen();
  b = daala_enc();
  fprintf(stdout, "Running the daala decoder %i times...", TIMES);
  fflush(stdout);
  daala = 0;
  for (i = 0; i < TIMES; i++) {
    fprintf(stdout, " %i", i + 1);
    fflush(stdout);
    daala += daala_dec(b);
  }
  printf("\ndaala %0.3f ms\n", daala/TIMES);
  b = vpx_enc();
  fprintf(stdout, "Running the vpx decoder %i times...", TIMES);
  fflush(stdout);
  vpx = 0;
  for (i = 0; i < TIMES; i++) {
    fprintf(stdout, " %i", i + 1);
    fflush(stdout);
    vpx += vpx_dec(b);
  }
  printf("\nvpx %0.3f ms\n", vpx/TIMES);
  b = ans_enc();
  fprintf(stdout, "Running the ans decoder %i times...", TIMES);
  fflush(stdout);
  ans = 0;
  for (i = 0; i < TIMES; i++) {
    fprintf(stdout, " %i", i + 1);
    fflush(stdout);
    ans += ans_dec(b);
  }
  printf("\nans %0.3f ms\n", ans/TIMES);
  printf("daala is %0.2f%% of vpx\n", (daala - vpx)/vpx*100 + 100);
  printf("ans is %0.2f%% of vpx\n", (ans - vpx)/vpx*100 + 100);
  return EXIT_SUCCESS;
}
