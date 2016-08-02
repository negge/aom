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
This file demonstrates the potential improvement when using alternative entropy
 coders instead of the vpxbool coder used in the AV1 code base by comparing the
 efficiency of coding a single symbol type using each entropy coder.
For this test, the symbol type considered is the one used for coding the intra
 prediction mode for an 8x8 block size.
To do the comparison, the aom_tree_index along with a set of aom_prob
 probabilities is converted into a CDF, a random set of 100M symbols is then
 drawn from that CDF, and encoded using the vpxbool, daala, and rans methods.
The results are printed at the end.

To compile this file, use the following commands:

./configure --enable-experimental --enable-daala_ec
gcc -I. -O2 ectest.c aom_dsp/prob.c aom_dsp/daalaboolwriter.c \
 aom_dsp/daalaboolreader.c aom_dsp/entenc.c aom_dsp/entdec.c \
 aom_dsp/entcode.c av1/encoder/treewriter.c aom_dsp/dkboolwriter.c \
 aom_dsp/dkboolreader.c av1/common/odintrin.c -o ectest
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <locale.h>
#include "av1/common/enums.h"
#include "aom_dsp/ansreader.h"
#include "aom_dsp/answriter.h"
#include "aom_dsp/daalaboolreader.h"
#include "aom_dsp/daalaboolwriter.h"
#include "aom_dsp/dkboolreader.h"
#include "aom_dsp/dkboolwriter.h"
#include "aom_dsp/prob.h"

/* Copied from av1/common/entropymode.h" */

#define BLOCK_SIZE_GROUPS 4

/* Copied from av1/common/entropymode.c */

/* Array indices are identical to previously-existing INTRAMODECONTEXTNODES. */
const aom_tree_index av1_intra_mode_tree[TREE_SIZE(INTRA_MODES)] = {
  -DC_PRED,   2,          /* 0 = DC_NODE */
  -TM_PRED,   4,          /* 1 = TM_NODE */
  -V_PRED,    6,          /* 2 = V_NODE */
  8,          12,         /* 3 = COM_NODE */
  -H_PRED,    10,         /* 4 = H_NODE */
  -D135_PRED, -D117_PRED, /* 5 = D135_NODE */
  -D45_PRED,  14,         /* 6 = D45_NODE */
  -D63_PRED,  16,         /* 7 = D63_NODE */
  -D153_PRED, -D207_PRED  /* 8 = D153_NODE */
};

static const aom_prob default_if_y_probs[BLOCK_SIZE_GROUPS][INTRA_MODES - 1] = {
  { 65, 32, 18, 144, 162, 194, 41, 51, 98 },   // block_size < 8x8
  { 132, 68, 18, 165, 217, 196, 45, 40, 78 },  // block_size < 16x16
  { 173, 80, 19, 176, 240, 193, 64, 35, 46 },  // block_size < 32x32
  { 221, 135, 38, 194, 248, 121, 96, 85, 29 }  // block_size >= 32x32
};

/* One Hundred Million Symbols */
#define NSYMBS (100000000)

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

void gen_symbs(int n, char *symbs, int nsymbs, const uint16_t *cdf) {
  int i;
  int j;
  int s;
  int count[16];
  setlocale(LC_NUMERIC, "");
  fprintf(stdout, "Generating %'d symbols...\n", NSYMBS);
  srand(0);
  for (i = 0; i < nsymbs; i++) {
    count[i] = 0;
  }
  for (i = 0; i < n; i++) {
    s = rand_int(32768);
    for (j = 0; j < nsymbs; j++) {
      if (s < cdf[j]) {
        symbs[i] = j;
        count[j]++;
        break;
      }
    }
  }
  for (i = 0; i < nsymbs; i++) {
    fprintf(stdout, "%i: %i (%lf)\n", i, count[i], ((double)count[i])/n);
  }
}

struct av1_token {
  int value;
  int len;
};

typedef struct aom_dk_writer aom_writer;

static INLINE void aom_write(aom_writer *br, int bit, int probability) {
  aom_dk_write(br, bit, probability);
}

static INLINE void aom_write_tree_bits(aom_writer *w, const aom_tree_index *tr,
                                       const aom_prob *probs, int bits, int len,
                                       aom_tree_index i) {
  do {
    const int bit = (bits >> --len) & 1;
    aom_write(w, bit, probs[i >> 1]);
    i = tr[i + bit];
  } while (len);
}

static INLINE void aom_write_tree(aom_writer *w, const aom_tree_index *tree,
                                  const aom_prob *probs, int bits, int len,
                                  aom_tree_index i) {
  aom_write_tree_bits(w, tree, probs, bits, len, i);
}

static INLINE void av1_write_token(aom_writer *w, const aom_tree_index *tree,
                                   const aom_prob *probs,
                                   const struct av1_token *token) {
  aom_write_tree(w, tree, probs, token->value, token->len, 0);
}

typedef struct aom_dk_reader aom_reader;

static INLINE int aom_read(aom_reader *r, int prob) {
  return aom_dk_read(r, prob);
}

static INLINE int aom_read_tree_bits(aom_reader *r, const aom_tree_index *tree,
                                     const aom_prob *probs) {
  aom_tree_index i = 0;

  while ((i = tree[i + aom_read(r, probs[i >> 1])]) > 0) continue;

  return -i;
}

static INLINE int aom_read_tree(aom_reader *r, const aom_tree_index *tree,
                                const aom_prob *probs) {
  return aom_read_tree_bits(r, tree, probs);
}

void rans_build_sym_tab(const uint16_t *cdf, struct rans_sym sym_tab[]) {
  int i;
  /* Convert Q15 to Q10 */
  sym_tab[0].prob = (cdf[0] + (1 << 4)) >> 5;
  sym_tab[0].cum_prob = 0;
  i = 0;
  do {
    i++;
    sym_tab[i].prob = (cdf[i] - cdf[i-1] + (1 << 4)) >> 5;
    sym_tab[i].cum_prob = sym_tab[i-1].cum_prob + sym_tab[i-1].prob;
  }
  while (cdf[i] < 32768u);
  /* Hack to make sure final cumulative probability equals 1 in Q10 */
  if (sym_tab[i].prob + sym_tab[i].cum_prob > 1024) {
    sym_tab[i].prob = 1024 - sym_tab[i].cum_prob;
  }
}

void rans_build_dec_tab(const struct rans_sym sym_tab[], rans_lut dec_tab) {
  int i;
  dec_tab[0] = 0;
  for (i = 1; dec_tab[i - 1] < RANS_PRECISION; ++i) {
    dec_tab[i] = dec_tab[i - 1] + sym_tab[i - 1].prob;
  }
}

#define BUF_SIZE (50000000)

#define SYMBOLS INTRA_MODES
#define TREE av1_intra_mode_tree
#define PROBS default_if_y_probs[0]

int main(int argc, char *argv[]) {
  uint16_t cdf[SYMBOLS];
  av1_tree_to_cdf(TREE, PROBS, cdf);
  int i;
  for (i = 0; i < SYMBOLS; i++) {
    int pdf;
    pdf = cdf[i];
    if (i > 0) {
      pdf -= cdf[i-1];
    }
    fprintf(stdout, "%i: %i (%lf)\n", i, pdf, pdf/32768.0);
  }
  /* Generate NSYMBS symbols based on this cdf. */
  char *symbs;
  symbs = malloc(NSYMBS);
  if (symbs == NULL) {
    fprintf(stderr, "Error allocating memory for symbols\n");
    return EXIT_FAILURE;
  }
  gen_symbs(NSYMBS, symbs, SYMBOLS, cdf);
  /* Encode NSYMBS using the Daala entropy coder. */
  uint8_t *buffer;
  buffer = malloc(BUF_SIZE);
  if (buffer == NULL) {
    fprintf(stderr, "Error allocating memory for EC buffer\n");
    return EXIT_FAILURE;
  }
  struct timeval t0, t1;
  gettimeofday(&t0, 0);
  daala_writer dw;
  aom_daala_start_encode(&dw, buffer);
  for (i = 0; i < NSYMBS; i++) {
    daala_write_symbol(&dw, symbs[i], cdf, SYMBOLS);
  }
  aom_daala_stop_encode(&dw);
  gettimeofday(&t1, 0);
  fprintf(stderr, "Daala EC size = %i (%lf ms)\n", dw.pos,
   (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f);
  /* Decode NSYMBS using the Daala entropy coder. */
  char *dsymbs;
  dsymbs = malloc(NSYMBS);
  if (dsymbs == NULL) {
    fprintf(stderr, "Error allocating memory for decoded symbols\n");
    return EXIT_FAILURE;
  }
  gettimeofday(&t0, 0);
  daala_reader dr;
  aom_daala_reader_init(&dr, buffer, dw.pos);
  for (i = 0; i < NSYMBS; i++) {
    dsymbs[i] = daala_read_tree_cdf(&dr, cdf, SYMBOLS);
  }
  gettimeofday(&t1, 0);
  int err;
  err = 0;
  for (i = 0; i < NSYMBS; i++) {
    if (symbs[i] != dsymbs[i]) {
      err++;
    }
  }
  fprintf(stderr, "Daala EC errors = %i (%lf ms)\n", err,
   (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f);
  /* Encode NSYMBS using the vpxbool entropy coder */
  struct av1_token tokens[SYMBOLS];
  av1_tokens_from_tree(tokens, TREE);
  int ind[SYMBOLS];
  int inv[SYMBOLS];
  av1_indices_from_tree(inv, ind, SYMBOLS, TREE);
  gettimeofday(&t0, 0);
  aom_dk_writer bw;
  aom_dk_start_encode(&bw, buffer);
  for (i = 0; i < NSYMBS; i++) {
    av1_write_token(&bw, TREE, PROBS, &tokens[ind[symbs[i]]]);
  }
  aom_dk_stop_encode(&bw);
  gettimeofday(&t1, 0);
  fprintf(stderr, "VPXbool EC size = %i (%lf ms)\n", bw.pos,
   (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f);
  /* Decode NSYMBS using the rANS entropy coder. */
  gettimeofday(&t0, 0);
  struct aom_dk_reader br;
  aom_dk_reader_init(&br, buffer, bw.pos, NULL, NULL);
  for (i = 0; i < NSYMBS; i++) {
    dsymbs[i] = inv[aom_read_tree(&br, TREE, PROBS)];
  }
  gettimeofday(&t1, 0);
  err = 0;
  for (i = 0; i < NSYMBS; i++) {
    if (symbs[i] != dsymbs[i]) {
      err++;
    }
  }
  fprintf(stderr, "VPXbool EC errors = %i (%lf ms)\n", err,
   (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f);
  /* Encode NSYMBS using the rANS entropy coder.
     Note the symbols are encoded in reverse order. */
  struct rans_sym rans_sym_tab[SYMBOLS];
  rans_build_sym_tab(cdf, rans_sym_tab);
  gettimeofday(&t0, 0);
  struct AnsCoder ac;
  ans_write_init(&ac, buffer);
  for (i = NSYMBS; i-- > 0; ) {
    rans_write(&ac, &rans_sym_tab[symbs[i]]);
  }
  int ans_size = ans_write_end(&ac);
  gettimeofday(&t1, 0);
  fprintf(stderr, "Ans EC size = %i (%lf ms)\n", ans_size,
   (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f);
  /* Decode NSYMBS using the rANS entropy coder. */
  rans_lut dec_tab;
  rans_build_dec_tab(rans_sym_tab, dec_tab);
  gettimeofday(&t0, 0);
  struct AnsDecoder ad;
  ans_read_init(&ad, buffer, ans_size);
  for (i = 0; i < NSYMBS; i++) {
    dsymbs[i] = rans_read(&ad, dec_tab);
  }
  gettimeofday(&t1, 0);
  err = 0;
  for (i = 0; i < NSYMBS; i++) {
    if (symbs[i] != dsymbs[i]) {
      err++;
    }
  }
  fprintf(stderr, "ANS EC errors = %i (%lf ms)\n", err,
   (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f);
  free(symbs);
  free(dsymbs);
  free(buffer);
  return EXIT_SUCCESS;
}
