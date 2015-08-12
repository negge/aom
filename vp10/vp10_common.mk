##
##  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##

VP10_COMMON_SRCS-yes += vp10_common.mk
VP10_COMMON_SRCS-yes += vp10_iface_common.h
VP10_COMMON_SRCS-yes += common/vp9_ppflags.h
VP10_COMMON_SRCS-yes += common/vp9_alloccommon.c
VP10_COMMON_SRCS-yes += common/vp9_blockd.c
VP10_COMMON_SRCS-yes += common/vp9_debugmodes.c
VP10_COMMON_SRCS-yes += common/vp9_entropy.c
VP10_COMMON_SRCS-yes += common/vp9_entropymode.c
VP10_COMMON_SRCS-yes += common/vp9_entropymv.c
VP10_COMMON_SRCS-yes += common/vp9_frame_buffers.c
VP10_COMMON_SRCS-yes += common/vp9_frame_buffers.h
VP10_COMMON_SRCS-yes += common/vp9_idct.c
VP10_COMMON_SRCS-yes += common/vp9_alloccommon.h
VP10_COMMON_SRCS-yes += common/vp9_blockd.h
VP10_COMMON_SRCS-yes += common/vp9_common.h
VP10_COMMON_SRCS-yes += common/vp9_entropy.h
VP10_COMMON_SRCS-yes += common/vp9_entropymode.h
VP10_COMMON_SRCS-yes += common/vp9_entropymv.h
VP10_COMMON_SRCS-yes += common/vp9_enums.h
VP10_COMMON_SRCS-yes += common/vp9_filter.h
VP10_COMMON_SRCS-yes += common/vp9_filter.c
VP10_COMMON_SRCS-yes += common/vp9_idct.h
VP10_COMMON_SRCS-yes += common/vp9_loopfilter.h
VP10_COMMON_SRCS-yes += common/vp9_thread_common.h
VP10_COMMON_SRCS-yes += common/vp9_mv.h
VP10_COMMON_SRCS-yes += common/vp9_onyxc_int.h
VP10_COMMON_SRCS-yes += common/vp9_pred_common.h
VP10_COMMON_SRCS-yes += common/vp9_pred_common.c
VP10_COMMON_SRCS-yes += common/vp9_quant_common.h
VP10_COMMON_SRCS-yes += common/vp9_reconinter.h
VP10_COMMON_SRCS-yes += common/vp9_reconintra.h
VP10_COMMON_SRCS-yes += common/vp10_rtcd.c
VP10_COMMON_SRCS-yes += common/vp10_rtcd_defs.pl
VP10_COMMON_SRCS-yes += common/vp9_scale.h
VP10_COMMON_SRCS-yes += common/vp9_scale.c
VP10_COMMON_SRCS-yes += common/vp9_seg_common.h
VP10_COMMON_SRCS-yes += common/vp9_seg_common.c
VP10_COMMON_SRCS-yes += common/vp9_systemdependent.h
VP10_COMMON_SRCS-yes += common/vp9_textblit.h
VP10_COMMON_SRCS-yes += common/vp9_tile_common.h
VP10_COMMON_SRCS-yes += common/vp9_tile_common.c
VP10_COMMON_SRCS-yes += common/vp9_loopfilter.c
VP10_COMMON_SRCS-yes += common/vp9_thread_common.c
VP10_COMMON_SRCS-yes += common/vp9_mvref_common.c
VP10_COMMON_SRCS-yes += common/vp9_mvref_common.h
VP10_COMMON_SRCS-yes += common/vp9_quant_common.c
VP10_COMMON_SRCS-yes += common/vp9_reconinter.c
VP10_COMMON_SRCS-yes += common/vp9_reconintra.c
VP10_COMMON_SRCS-$(CONFIG_POSTPROC_VISUALIZER) += common/vp9_textblit.c
VP10_COMMON_SRCS-yes += common/vp9_common_data.c
VP10_COMMON_SRCS-yes += common/vp9_common_data.h
VP10_COMMON_SRCS-yes += common/vp9_scan.c
VP10_COMMON_SRCS-yes += common/vp9_scan.h

VP10_COMMON_SRCS-$(CONFIG_VP9_POSTPROC) += common/vp9_postproc.h
VP10_COMMON_SRCS-$(CONFIG_VP9_POSTPROC) += common/vp9_postproc.c
VP10_COMMON_SRCS-$(CONFIG_VP9_POSTPROC) += common/vp9_mfqe.h
VP10_COMMON_SRCS-$(CONFIG_VP9_POSTPROC) += common/vp9_mfqe.c
ifeq ($(CONFIG_VP9_POSTPROC),yes)
VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/vp9_mfqe_sse2.asm
VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/vp9_postproc_sse2.asm
endif

ifneq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
VP10_COMMON_SRCS-$(HAVE_DSPR2)  += common/mips/dspr2/vp9_itrans4_dspr2.c
VP10_COMMON_SRCS-$(HAVE_DSPR2)  += common/mips/dspr2/vp9_itrans8_dspr2.c
VP10_COMMON_SRCS-$(HAVE_DSPR2)  += common/mips/dspr2/vp9_itrans16_dspr2.c
endif

# common (msa)
VP10_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/vp9_idct4x4_msa.c
VP10_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/vp9_idct8x8_msa.c
VP10_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/vp9_idct16x16_msa.c

ifeq ($(CONFIG_VP9_POSTPROC),yes)
VP10_COMMON_SRCS-$(HAVE_MSA) += common/mips/msa/vp9_mfqe_msa.c
endif

VP10_COMMON_SRCS-$(HAVE_SSE2) += common/x86/vp9_idct_intrin_sse2.c

ifneq ($(CONFIG_VP9_HIGHBITDEPTH),yes)
VP10_COMMON_SRCS-$(HAVE_NEON) += common/arm/neon/vp9_iht4x4_add_neon.c
VP10_COMMON_SRCS-$(HAVE_NEON) += common/arm/neon/vp9_iht8x8_add_neon.c
endif

$(eval $(call rtcd_h_template,vp10_rtcd,vp10/common/vp10_rtcd_defs.pl))