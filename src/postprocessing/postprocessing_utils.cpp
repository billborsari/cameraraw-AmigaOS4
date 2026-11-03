/* -*- C++ -*-
 * Copyright 2019-2025 LibRaw LLC (info@libraw.org)
 *
 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/libraw_cxx_defs.h"

#define TBLN 65535

void LibRaw::exp_bef(float shift, float smooth)
{
  // params limits
  if (shift > 8)
    shift = 8;
  if (shift < 0.25)
    shift = 0.25;
  if (smooth < 0.0)
    smooth = 0.0;
  if (smooth > 1.0)
    smooth = 1.0;

  unsigned short *lut = (ushort *)malloc((TBLN + 1) * sizeof(unsigned short));

  if (shift <= 1.0)
  {
    for (int i = 0; i <= TBLN; i++)
      lut[i] = (unsigned short)((float)i * shift);
  }
  else
  {
    float x1, x2, y1, y2;

    float cstops = log(shift) / log(2.0f);
    float room = cstops * 2;
    float roomlin = powf(2.0f, room);
    x2 = (float)TBLN;
    x1 = (x2 + 1) / roomlin - 1;
    y1 = x1 * shift;
    y2 = x2 * (1 + (1 - smooth) * (shift - 1));
    float sq3x = powf(x1 * x1 * x2, 1.0f / 3.0f);
    float B = (y2 - y1 + shift * (3 * x1 - 3.0f * sq3x)) /
              (x2 + 2.0f * x1 - 3.0f * sq3x);
    float A = (shift - B) * 3.0f * powf(x1 * x1, 1.0f / 3.0f);
    float CC = y2 - A * powf(x2, 1.0f / 3.0f) - B * x2;
    for (int i = 0; i <= TBLN; i++)
    {
      float X = (float)i;
      float Y = A * powf(X, 1.0f / 3.0f) + B * X + CC;
      if (i < x1)
        lut[i] = (unsigned short)((float)i * shift);
      else
        lut[i] = Y < 0 ? 0 : (Y > TBLN ? TBLN : (unsigned short)(Y));
    }
  }
  for (int i = 0; i < S.height * S.width; i++)
  {
    imgdata.image[i][0] = lut[imgdata.image[i][0]];
    imgdata.image[i][1] = lut[imgdata.image[i][1]];
    imgdata.image[i][2] = lut[imgdata.image[i][2]];
    imgdata.image[i][3] = lut[imgdata.image[i][3]];
  }

  if (C.data_maximum <= TBLN)
    C.data_maximum = lut[C.data_maximum];
  if (C.maximum <= TBLN)
    C.maximum = lut[C.maximum];
  free(lut);
}

void LibRaw::convert_to_rgb_loop(float out_cam[3][4])
{
  int row, col, c;
  float out[3];
  ushort *img;
  memset(libraw_internal_data.output_data.histogram, 0,
         sizeof(int) * LIBRAW_HISTOGRAM_SIZE * 4);
  if (libraw_internal_data.internal_output_params.raw_color)
  {
    for (img = imgdata.image[0], row = 0; row < S.height; row++)
    {
      for (col = 0; col < S.width; col++, img += 4)
      {
        for (c = 0; c < imgdata.idata.colors; c++)
        {
          libraw_internal_data.output_data.histogram[c][img[c] >> 3]++;
        }
      }
    }
  }
  else if (imgdata.idata.colors == 3)
  {
    for (img = imgdata.image[0], row = 0; row < S.height; row++)
    {
      for (col = 0; col < S.width; col++, img += 4)
      {
        out[0] = out_cam[0][0] * img[0] + out_cam[0][1] * img[1] +
                 out_cam[0][2] * img[2];
        out[1] = out_cam[1][0] * img[0] + out_cam[1][1] * img[1] +
                 out_cam[1][2] * img[2];
        out[2] = out_cam[2][0] * img[0] + out_cam[2][1] * img[1] +
                 out_cam[2][2] * img[2];
        img[0] = CLIP((int)out[0]);
        img[1] = CLIP((int)out[1]);
        img[2] = CLIP((int)out[2]);
        libraw_internal_data.output_data.histogram[0][img[0] >> 3]++;
        libraw_internal_data.output_data.histogram[1][img[1] >> 3]++;
        libraw_internal_data.output_data.histogram[2][img[2] >> 3]++;
      }
    }
  }
  else if (imgdata.idata.colors == 4)
  {
    for (img = imgdata.image[0], row = 0; row < S.height; row++)
    {
      for (col = 0; col < S.width; col++, img += 4)
      {
        out[0] = out_cam[0][0] * img[0] + out_cam[0][1] * img[1] +
                 out_cam[0][2] * img[2] + out_cam[0][3] * img[3];
        out[1] = out_cam[1][0] * img[0] + out_cam[1][1] * img[1] +
                 out_cam[1][2] * img[2] + out_cam[1][3] * img[3];
        out[2] = out_cam[2][0] * img[0] + out_cam[2][1] * img[1] +
                 out_cam[2][2] * img[2] + out_cam[2][3] * img[3];
        img[0] = CLIP((int)out[0]);
        img[1] = CLIP((int)out[1]);
        img[2] = CLIP((int)out[2]);
        libraw_internal_data.output_data.histogram[0][img[0] >> 3]++;
        libraw_internal_data.output_data.histogram[1][img[1] >> 3]++;
        libraw_internal_data.output_data.histogram[2][img[2] >> 3]++;
        libraw_internal_data.output_data.histogram[3][img[3] >> 3]++;
      }
    }
  }
}

/* cameraraw-AmigaOS4: the two loops below compute, per channel, a pure
 * function of the 16-bit input value: int((v - cblack[c]) * scale_mul[c])
 * clipped, with 0 left at 0 when there is a black level. Tabulating it once
 * per channel gives bit-identical output and replaces an int->float
 * conversion, a float multiply and a float->int conversion per sample with a
 * table load. On 32-bit PowerPC both conversions go through memory (no
 * fcfid / direct GPR<->FPR moves), and under QEMU every float op is a helper
 * call. The tables only cover 0..max(maximum, data_maximum) (8-32 KB per
 * channel for 12-14 bit data) so they stay in cache; larger values (rare)
 * take the arithmetic path. Enabled with LIBRAW_SCALE_COLORS_LUT. */
static inline ushort scale_one(int val, int black, float mul, bool bl)
{
  if (bl)
  {
    if (!val)
      return 0;
    val -= black;
  }
  val = int(val * mul);
  return CLIP(val);
}

bool LibRaw::scale_colors_lut(float scale_mul[4], unsigned size)
{
  unsigned lim = MAX((unsigned)C.maximum, (unsigned)C.data_maximum) + 1;
  if (lim < 2 || lim > 0x10000)
    lim = 0x10000;
  ushort *lut = (ushort *)malloc(4 * lim * sizeof(ushort));
  if (!lut)
    return false;
  const bool bl = C.cblack[0] || C.cblack[1] || C.cblack[2] || C.cblack[3];
  for (unsigned c = 0; c < 4; c++)
    for (unsigned v = 0; v < lim; v++)
      lut[c * lim + v] = scale_one(v, C.cblack[c], scale_mul[c], bl);
  ushort(*img)[4] = imgdata.image;
  for (unsigned i = 0; i < size; i++)
    for (unsigned c = 0; c < 4; c++)
    {
      unsigned v = img[i][c];
      img[i][c] = v < lim ? lut[c * lim + v] : scale_one(v, C.cblack[c], scale_mul[c], bl);
    }
  free(lut);
  return true;
}

void LibRaw::scale_colors_loop(float scale_mul[4])
{
  unsigned size = S.iheight * S.iwidth;

  if (C.cblack[4] && C.cblack[5])
  {
    int val;
    for (unsigned i = 0; i < size; i++)
    {
      for (unsigned c = 0; c < 4; c++)
      {
        if (!(val = imgdata.image[i][c])) continue;
        val -= C.cblack[6 + i / S.iwidth % C.cblack[4] * C.cblack[5] +
                        i % S.iwidth % C.cblack[5]];
        val -= C.cblack[c];
        val = int(val * scale_mul[c]);
        imgdata.image[i][c] = CLIP(val);
      }
    }
  }
#ifdef LIBRAW_SCALE_COLORS_LUT
  else if (size >= 0x20000 && scale_colors_lut(scale_mul, size))
#else
  else if (0)
#endif
  {
    /* done with lookup tables (cameraraw-AmigaOS4) */
  }
  else if (C.cblack[0] || C.cblack[1] || C.cblack[2] || C.cblack[3])
  {
    for (unsigned i = 0; i < size; i++)
    {
      for (unsigned c = 0; c < 4; c++)
      {
        int val = imgdata.image[i][c];
        if (!val) continue;
        val -= C.cblack[c];
        val = int(val * scale_mul[c]);
        imgdata.image[i][c] = CLIP(val);
      }
    }
  }
  else // BL is zero
  {
    for (unsigned i = 0; i < size; i++)
    {
      for (unsigned c = 0; c < 4; c++)
      {
        int val = imgdata.image[i][c];
        val = int(val * scale_mul[c]);
        imgdata.image[i][c] = CLIP(val);
      }
    }
  }
}
