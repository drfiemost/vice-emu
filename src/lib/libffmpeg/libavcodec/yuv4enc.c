/*
 * libquicktime yuv4 encoder
 *
 * Copyright (c) 2011 Carl Eugen Hoyos
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avcodec.h"
#include "internal.h"

static av_cold int yuv4_encode_init(AVCodecContext *avctx)
{
    avctx->coded_frame = av_frame_alloc();

    if (!avctx->coded_frame) {
        av_log(avctx, AV_LOG_ERROR, "Could not allocate frame.\n");
        return AVERROR(ENOMEM);
    }

    return 0;
}

static int yuv4_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                             const AVFrame *pic, int *got_packet)
{
    uint8_t *dst;
    uint8_t *y, *u, *v;
    int i, j, ret;

    if ((ret = ff_alloc_packet2(avctx, pkt, 6 * (avctx->width + 1 >> 1) * (avctx->height + 1 >> 1))) < 0)
        return ret;
    dst = pkt->data;

    avctx->coded_frame->key_frame = 1;
    avctx->coded_frame->pict_type = AV_PICTURE_TYPE_I;

    y = pic->data[0];
    u = pic->data[1];
    v = pic->data[2];

    for (i = 0; i < avctx->height + 1 >> 1; i++) {
        for (j = 0; j < avctx->width + 1 >> 1; j++) {
            *dst++ = u[j] ^ 0x80;
            *dst++ = v[j] ^ 0x80;
            *dst++ = y[                   2 * j    ];
            *dst++ = y[                   2 * j + 1];
            *dst++ = y[pic->linesize[0] + 2 * j    ];
            *dst++ = y[pic->linesize[0] + 2 * j + 1];
        }
        y += 2 * pic->linesize[0];
        u +=     pic->linesize[1];
        v +=     pic->linesize[2];
    }

    pkt->flags |= AV_PKT_FLAG_KEY;
    *got_packet = 1;
    return 0;
}

static av_cold int yuv4_encode_close(AVCodecContext *avctx)
{
    av_freep(&avctx->coded_frame);

    return 0;
}

#ifdef IDE_COMPILE
static const enum AVPixelFormat tmp1[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
#endif

AVCodec ff_yuv4_encoder = {
#ifdef IDE_COMPILE
    "yuv4",
    "Uncompressed packed 4:2:0",
    AVMEDIA_TYPE_VIDEO,
    AV_CODEC_ID_YUV4,
    0, 0, tmp1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, yuv4_encode_init,
    0, yuv4_encode_frame,
    0, yuv4_encode_close,
#else
	.name         = "yuv4",
    .long_name    = NULL_IF_CONFIG_SMALL("Uncompressed packed 4:2:0"),
    .type         = AVMEDIA_TYPE_VIDEO,
    .id           = AV_CODEC_ID_YUV4,
    .init         = yuv4_encode_init,
    .encode2      = yuv4_encode_frame,
    .close        = yuv4_encode_close,
    .pix_fmts     = (const enum AVPixelFormat[]){ AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE },
#endif
};
