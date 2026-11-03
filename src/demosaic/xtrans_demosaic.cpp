/* -*- C++ -*-
 * Copyright 2019-2025 LibRaw LLC (info@libraw.org)
 *
 LibRaw uses code from dcraw.c -- Dave Coffin's raw photo decoder,
 dcraw.c is copyright 1997-2018 by Dave Coffin, dcoffin a cybercom o net.
 LibRaw do not use RESTRICTED code from dcraw.c

 LibRaw is free software; you can redistribute it and/or modify
 it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include "../../internal/dcraw_defs.h"

#define fcol(row, col) xtrans[(row + 6) % 6][(col + 6) % 6]

#ifdef LIBRAW_XTRANS_INTCMP
/* Order-preserving float <-> int mapping for finite, non-NaN floats
 * (negative values have their magnitude bits flipped). may_alias: the keys
 * are stored over the float drv[] buffer. */
typedef int __attribute__((__may_alias__)) xtrans_key;
static inline int xtrans_fkey(float f)
{
  int k;
  memcpy(&k, &f, sizeof k);
  return k >= 0 ? k : k ^ 0x7FFFFFFF;
}
static inline float xtrans_keyf(int k)
{
  float f;
  k = k >= 0 ? k : k ^ 0x7FFFFFFF;
  memcpy(&f, &k, sizeof f);
  return f;
}
#endif
/*
   Frank Markesteijn's algorithm for Fuji X-Trans sensors
 */
void LibRaw::xtrans_interpolate(int passes)
{
  int cstat[4] = {0, 0, 0, 0};
  int ndir;
  static const short orth[12] = {1, 0, 0, 1, -1, 0, 0, -1, 1, 0, 0, 1},
                     patt[2][16] = {{0, 1, 0, -1, 2, 0, -1, 0, 1, 1, 1, -1, 0,
                                     0, 0, 0},
                                    {0, 1, 0, -2, 1, 0, -2, 0, 1, 1, -2, -2, 1,
                                     -1, -1, 1}},
                     dir[4] = {1, LIBRAW_AHD_TILE, LIBRAW_AHD_TILE + 1,
                               LIBRAW_AHD_TILE - 1};
  short allhex[3][3][2][8];
  ushort sgrow = 0, sgcol = 0;

  if (width < LIBRAW_AHD_TILE || height < LIBRAW_AHD_TILE || filters != 9)
    throw LIBRAW_EXCEPTION_IO_CORRUPT; // too small image
                                       /* Check against right pattern */
  for (int row = 0; row < 6; row++)
    for (int col = 0; col < 6; col++)
      cstat[(unsigned)fcol(row, col)]++;

  if (cstat[0] < 6 || cstat[0] > 10 || cstat[1] < 16 || cstat[1] > 24 ||
      cstat[2] < 6 || cstat[2] > 10 || cstat[3])
    throw LIBRAW_EXCEPTION_IO_CORRUPT;

  // Init allhex table to unreasonable values
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 2; k++)
        for (int l = 0; l < 8; l++)
          allhex[i][j][k][l] = 32700;

  cielab(0, 0);
  ndir = 4 << int(passes > 1);

  int minv = 0, maxv = 0, minh = 0, maxh = 0;
  /* Map a green hexagon around each non-green pixel and vice versa:	*/
  for (int row = 0; row < 3; row++)
    for (int col = 0; col < 3; col++)
      for (int ng = 0, d = 0; d < 10; d += 2)
      {
        int g = fcol(row, col) == 1;
        if (fcol(row + orth[d], col + orth[d + 2]) == 1)
          ng = 0;
        else
          ng++;
        if (ng == 4)
        {
          sgrow = row;
          sgcol = col;
        }
        if (ng == g + 1)
        {
            int c;
            FORC(8)
            {
                int v = orth[d] * patt[g][c * 2] + orth[d + 1] * patt[g][c * 2 + 1];
                int h = orth[d + 2] * patt[g][c * 2] + orth[d + 3] * patt[g][c * 2 + 1];
                minv = MIN(v, minv);
                maxv = MAX(v, maxv);
                minh = MIN(v, minh);
                maxh = MAX(v, maxh);
                allhex[row][col][0][c ^ (g * 2 & d)] = h + v * width;
                allhex[row][col][1][c ^ (g * 2 & d)] = h + v * LIBRAW_AHD_TILE;
            }
        }
      }

  // Check allhex table initialization
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      for (int k = 0; k < 2; k++)
        for (int l = 0; l < 8; l++)
          if (allhex[i][j][k][l] > maxh + maxv * width + 1 ||
              allhex[i][j][k][l] < minh + minv * width - 1)
            throw LIBRAW_EXCEPTION_IO_CORRUPT;
  int retrycount = 0;

  /* Set green1 and green3 to the minimum and maximum allowed values:	*/
  for (int row = 2; row < height - 2; row++)
  {
      int col;
      ushort min, max;
      for (col = 2, max = 0u, min = 0xffffu; col < int(width) - 2; col++)
      {
          if (fcol(row, col) == 1 && (min = ~(max = 0)))
              continue;
          ushort(*pix)[4];
          pix = image + row * width + col;
          short* hex = allhex[row % 3][col % 3][0];
          if (!max)
          {
              int c;
              FORC(6)
              {
                  int val = pix[hex[c]][1];
                  if (min > val)
                      min = val;
                  if (max < val)
                      max = val;
              }
          }
          pix[0][1] = min;
          pix[0][3] = max;
          switch ((row - sgrow) % 3)
          {
          case 1:
              if (row < height - 3)
              {
                  row++;
                  col--;
              }
              break;
          case 2:
              if ((min = ~(max = 0)) && (col += 2) < width - 3 && row > 2)
              {
                  row--;
                  if (retrycount++ > width * height)
                      throw LIBRAW_EXCEPTION_IO_CORRUPT;
              }
          }
      }
  }

  for (int row = 3; row < 9 && row < height - 3; row++)
	  for (int col = 3; col < 9 && col < width - 3; col++)
	  {
		  if ((fcol(row, col)) == 1)
			  continue;
          short* hex = allhex[row % 3][col % 3][0];
          int c;
		  FORC(2)
		  {
			  int idx3 = 3 * hex[4 + c] + row * width + col;
			  int idx4 = -3 * hex[4 + c] + row * width + col;
			  int maxidx = width * height;
			  if (idx3 < 0 || idx3 >= maxidx)
				  throw LIBRAW_EXCEPTION_IO_CORRUPT;
			  if (idx4 < 0 || idx4 >= maxidx)
				  throw LIBRAW_EXCEPTION_IO_CORRUPT;
		  }
	  }

#if defined(LIBRAW_USE_OPENMP)
  int buffer_count = omp_get_max_threads();
#else
  int buffer_count = 1;
#endif

  size_t buffer_size = LIBRAW_AHD_TILE * LIBRAW_AHD_TILE * (ndir * 11 + 6);
  char** buffers = malloc_omp_buffers(buffer_count, buffer_size);
#ifdef LIBRAW_XTRANS_BOXSUM
#if defined(LIBRAW_USE_OPENMP)
#error LIBRAW_XTRANS_BOXSUM: per-thread box buffers not implemented
#endif
  /* 5x5 sums of homo[] are at most 25 * 9 = 225: one byte each. */
  unsigned char(*box)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE] =
      (unsigned char(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE])malloc(ndir * LIBRAW_AHD_TILE * LIBRAW_AHD_TILE);
  if (!box)
  {
    free_omp_buffers(buffers, buffer_count);
    throw LIBRAW_EXCEPTION_ALLOC;
  }
  int vsum[LIBRAW_AHD_TILE + 8];
#endif

#if defined(LIBRAW_USE_OPENMP)
# pragma omp parallel for schedule(dynamic) default(none) firstprivate(buffers, allhex, passes, sgrow, sgcol, ndir) shared(dir) 
#endif
    for (int top = 3; top < height - 19; top += LIBRAW_AHD_TILE - 16)
    {
#if defined(LIBRAW_USE_OPENMP)
        char* buffer = buffers[omp_get_thread_num()];
#else
        char* buffer = buffers[0];
#endif

        ushort(*rgb)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3], (*rix)[3];
        short(*lab)[LIBRAW_AHD_TILE][3], (*lix)[3];
        float(*drv)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE];
        char(*homo)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE];

        rgb = (ushort(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3])buffer;
        lab = (short(*)[LIBRAW_AHD_TILE][3])(
            buffer + LIBRAW_AHD_TILE * LIBRAW_AHD_TILE * (ndir * 6));
        drv = (float(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE])(
            buffer + LIBRAW_AHD_TILE * LIBRAW_AHD_TILE * (ndir * 6 + 6));
        homo = (char(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE])(
            buffer + LIBRAW_AHD_TILE * LIBRAW_AHD_TILE * (ndir * 10 + 6));

        for (int left = 3; left < width - 19; left += LIBRAW_AHD_TILE - 16)
        {
            int mrow = MIN(top + LIBRAW_AHD_TILE, height - 3);
            int mcol = MIN(left + LIBRAW_AHD_TILE, width - 3);
            for (int row = top; row < mrow; row++)
                for (int col = left; col < mcol; col++)
                    memcpy(rgb[0][row - top][col - left], image[row * width + col], 6);
            int c;
            FORC3 memcpy(rgb[c + 1], rgb[0], sizeof * rgb);

            /* Interpolate green horizontally, vertically, and along both diagonals:
             */
            int color[3][8];
            for (int row = top; row < mrow; row++)
                for (int col = left; col < mcol; col++)
                {
                    int f;
                    if ((f = fcol(row, col)) == 1)
                        continue;
                    ushort (*pix)[4] = image + row * width + col;
                    short* hex = allhex[row % 3][col % 3][0];
                    color[1][0] = 174 * (pix[hex[1]][1] + pix[hex[0]][1]) -
                        46 * (pix[2 * hex[1]][1] + pix[2 * hex[0]][1]);
                    color[1][1] = 223 * pix[hex[3]][1] + pix[hex[2]][1] * 33 +
                        92 * (pix[0][f] - pix[-hex[2]][f]);
                    FORC(2)
                        color[1][2 + c] = 164 * pix[hex[4 + c]][1] +
                        92 * pix[-2 * hex[4 + c]][1] +
                        33 * (2 * pix[0][f] - pix[3 * hex[4 + c]][f] -
                            pix[-3 * hex[4 + c]][f]);
                    FORC4 rgb[c ^ !((row - sgrow) % 3)][row - top][col - left][1] =
                        LIM(color[1][c] >> 8, pix[0][1], pix[0][3]);
                }

            for (int pass = 0; pass < passes; pass++)
            {
                if (pass == 1)
                    memcpy(rgb += 4, buffer, 4 * sizeof * rgb);

                /* Recalculate green from interpolated values of closer pixels:	*/
                if (pass)
                {
                    for (int row = top + 2; row < mrow - 2; row++)
                        for (int col = left + 2; col < mcol - 2; col++)
                        {
                            int f;
                            if ((f = fcol(row, col)) == 1)
                                continue;
                            ushort(*pix)[4] = image + row * width + col;
                            short* hex = allhex[row % 3][col % 3][1];
                            for (int d = 3; d < 6; d++)
                            {
                                rix =
                                    &rgb[(d - 2) ^ !((row - sgrow) % 3)][row - top][col - left];
                                int val = rix[-2 * hex[d]][1] + 2 * rix[hex[d]][1] -
                                    rix[-2 * hex[d]][f] - 2 * rix[hex[d]][f] + 3 * rix[0][f];
                                rix[0][1] = LIM(val / 3, pix[0][1], pix[0][3]);
                            }
                        }
                }

                /* Interpolate red and blue values for solitary green pixels:	*/
                for (int row = (top - sgrow + 4) / 3 * 3 + sgrow; row < mrow - 2; row += 3)
                    for (int col = (left - sgcol + 4) / 3 * 3 + sgcol; col < mcol - 2; col += 3)
                {
                    rix = &rgb[0][row - top][col - left];
                    int h = fcol(row, col + 1);

					if (h == 1) // Incorrect pattern
                      break;

                    float diff[6];
                    memset(diff, 0, sizeof diff);
                    for (int i = 1, d = 0; d < 6; d++, i ^= LIBRAW_AHD_TILE ^ 1, h ^= 2)
                    {
                        for (c = 0; c < 2; c++, h ^= 2)
                        {
                            int g = 2 * rix[0][1] - rix[i << c][1] - rix[-i << c][1];
                            color[h][d] = g + rix[i << c][h] + rix[-i << c][h];
                            if (d > 1)
                                diff[d] += SQR((float)rix[i << c][1] - (float)rix[-i << c][1] -
                                    (float)rix[i << c][h] + (float)rix[-i << c][h]) + SQR((float)g);
                        }
                        if (d > 1 && (d & 1))
                            if (diff[d - 1] < diff[d])
                                FORC(2) color[c * 2][d] = color[c * 2][d - 1];
                        if (d < 2 || (d & 1))
                        {
                            FORC(2) rix[0][c * 2] = CLIP(color[c * 2][d] / 2);
                            rix += LIBRAW_AHD_TILE * LIBRAW_AHD_TILE;
                        }
                    }
                }

                /* Interpolate red for blue pixels and vice versa:		*/
                for (int row = top + 3; row < mrow - 3; row++)
                    for (int col = left + 3; col < mcol - 3; col++)
                    {
                        int f;
                        if ((f = 2 - fcol(row, col)) == 1)
                            continue;
                        rix = &rgb[0][row - top][col - left];
                        c = (row - sgrow) % 3 ? LIBRAW_AHD_TILE : 1;
                        int h = 3 * (c ^ LIBRAW_AHD_TILE ^ 1);
                        for (int d = 0; d < 4; d++, rix += LIBRAW_AHD_TILE * LIBRAW_AHD_TILE)
                        {
                            int i = d > 1 || ((d ^ c) & 1) ||
                                ((ABS(rix[0][1] - rix[c][1]) +
                                    ABS(rix[0][1] - rix[-c][1])) <
                                    2 * (ABS(rix[0][1] - rix[h][1]) +
                                        ABS(rix[0][1] - rix[-h][1])))
                                ? c
                                : h;
                            rix[0][f] = CLIP((rix[i][f] + rix[-i][f] + 2 * rix[0][1] -
                                rix[i][1] - rix[-i][1]) /
                                2);
                        }
                    }

                /* Fill in red and blue for 2x2 blocks of green:		*/
                for (int row = top + 2; row < mrow - 2; row++)
                    if ((row - sgrow) % 3)
                        for (int col = left + 2; col < mcol - 2; col++)
                            if ((col - sgcol) % 3)
                            {
                                rix = &rgb[0][row - top][col - left];
                                short* hex = allhex[row % 3][col % 3][1];
                                for (int d = 0; d < 8;
                                    d += 2, rix += LIBRAW_AHD_TILE * LIBRAW_AHD_TILE)
                                    if (hex[d] + hex[d + 1])
                                    {
                                        int g = 3 * rix[0][1] - 2 * rix[hex[d]][1] - rix[hex[d + 1]][1];
                                        for (c = 0; c < 4; c += 2)
                                            rix[0][c] = CLIP(
                                                (g + 2 * rix[hex[d]][c] + rix[hex[d + 1]][c]) / 3);
                                    }
                                    else
                                    {
                                        int g = 2 * rix[0][1] - rix[hex[d]][1] - rix[hex[d + 1]][1];
                                        for (c = 0; c < 4; c += 2)
                                            rix[0][c] =
                                            CLIP((g + rix[hex[d]][c] + rix[hex[d + 1]][c]) / 2);
                                    }
                            }
            }
            rgb = (ushort(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE][3])buffer;
            mrow -= top;
            mcol -= left;

            /* Convert to CIELab and differentiate in all directions:	*/
            // no effect
            for (int d = 0; d < ndir; d++)
            {
                for (int row = 2; row < mrow - 2; row++)
                    for (int col = 2; col < mcol - 2; col++)
                        cielab(rgb[d][row][col], lab[row][col]);
                for (int f = dir[d & 3], row = 3; row < mrow - 3; row++)
                    for (int col = 3; col < mcol - 3; col++)
                    {
                        lix = &lab[row][col];
                        int g = 2 * lix[0][0] - lix[f][0] - lix[-f][0];
                        drv[d][row][col] =
							float(
                            SQR(g) +
                            SQR((2 * lix[0][1] - lix[f][1] - lix[-f][1] + g * 500 / 232)) +
                            SQR((2 * lix[0][2] - lix[f][2] - lix[-f][2] - g * 500 / 580))
								);
                    }
            }

            /* Build homogeneity maps from the derivatives:			*/
            memset(homo, 0, ndir * LIBRAW_AHD_TILE * LIBRAW_AHD_TILE);
#ifdef LIBRAW_XTRANS_INTCMP
            /* cameraraw-AmigaOS4: same comparisons on integers. drv[] holds
             * finite floats (no NaN, no -0), and xtrans_fkey() maps them to
             * ints in the same order, so "drv <= tr" == "key <= key(tr)".
             * The keys overwrite drv[] in place (it isn't used again); only
             * tr *= 8 stays a float op. 9 * ndir float compares per pixel
             * become integer ones: a big saving under QEMU, where each FP
             * instruction is a helper call. */
            {
                xtrans_key(*drvk)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE] =
                    (xtrans_key(*)[LIBRAW_AHD_TILE][LIBRAW_AHD_TILE])drv;
                for (int d = 0; d < ndir; d++)
                    for (int row = 3; row < mrow - 3; row++)
                        for (int col = 3; col < mcol - 3; col++)
                            drvk[d][row][col] = xtrans_fkey(drv[d][row][col]);
                for (int row = 4; row < mrow - 4; row++)
                    for (int col = 4; col < mcol - 4; col++)
                    {
                        int trk = drvk[0][row][col];
                        for (int d = 1; d < ndir; d++)
                            if (trk > drvk[d][row][col])
                                trk = drvk[d][row][col];
                        trk = xtrans_fkey(xtrans_keyf(trk) * 8);
                        for (int dd = 0; dd < ndir; dd++)
                        {
                            int n = 0;
                            for (int v = -1; v <= 1; v++)
                                for (int h = -1; h <= 1; h++)
                                    n += drvk[dd][row + v][col + h] <= trk;
                            homo[dd][row][col] = n;
                        }
                    }
            }
#else
            for (int row = 4; row < mrow - 4; row++)
                for (int col = 4; col < mcol - 4; col++)
                {
                    int d;
                    float tr;
                    for (tr = FLT_MAX, d = 0; d < ndir; d++)
                        if (tr > drv[d][row][col])
                            tr = drv[d][row][col];
                    tr *= 8;
                    for (int dd = 0; dd < ndir; dd++)
                        for (int v = -1; v <= 1; v++)
                            for (int h = -1; h <= 1; h++)
                                if (drv[dd][row + v][col + h] <= tr)
                                    homo[dd][row][col]++;
                }
#endif

            /* Average the most homogeneous pixels for the final result:	*/
            if (height - top < LIBRAW_AHD_TILE + 4)
                mrow = height - top + 2;
            if (width - left < LIBRAW_AHD_TILE + 4)
                mcol = width - left + 2;
#ifdef LIBRAW_XTRANS_BOXSUM
            /* cameraraw-AmigaOS4: the 5x5 sums of homo[] below, computed
             * once per tile as a separable running box filter (5+2 adds per
             * pixel and direction instead of 25). Integer sums of the same
             * values, so the result is identical. */
            {
                const int r0 = MIN(top, 8), r1 = mrow - 8;
                const int c0 = MIN(left, 8), c1 = mcol - 8;
                for (int d = 0; d < ndir && c1 > c0; d++)
                    for (int row = r0; row < r1; row++)
                    {
                        for (int col = c0 - 2; col < c1 + 2; col++)
                            vsum[col] = homo[d][row - 2][col] + homo[d][row - 1][col] +
                                        homo[d][row][col] + homo[d][row + 1][col] +
                                        homo[d][row + 2][col];
                        int sum = vsum[c0 - 2] + vsum[c0 - 1] + vsum[c0] + vsum[c0 + 1] + vsum[c0 + 2];
                        box[d][row][c0] = sum;
                        for (int col = c0 + 1; col < c1; col++)
                        {
                            sum += vsum[col + 2] - vsum[col - 3];
                            box[d][row][col] = sum;
                        }
                    }
            }
#endif
            for (int row = MIN(top, 8); row < mrow - 8; row++)
                for (int col = MIN(left, 8); col < mcol - 8; col++)
                {
                    int v;
                    int hm[8];
#ifdef LIBRAW_XTRANS_BOXSUM
                    (void)v;
                    for (int d = 0; d < ndir; d++)
                        hm[d] = box[d][row][col];
#else
                    for (int d = 0; d < ndir; d++)
                        for (v = -2, hm[d] = 0; v <= 2; v++)
                            for (int h = -2; h <= 2; h++)
                                hm[d] += homo[d][row + v][col + h];
#endif
                    for (int d = 0; d < ndir - 4; d++)
                        if (hm[d] < hm[d + 4])
                            hm[d] = 0;
                        else if (hm[d] > hm[d + 4])
                            hm[d + 4] = 0;
                    ushort max;
                    int d;
                    for (d = 1, max = hm[0]; d < ndir; d++)
                        if (max < hm[d])
                            max = hm[d];
                    max -= max >> 3;

                    int avg[4];
                    memset(avg, 0, sizeof avg);
                    for (int dd = 0; dd < ndir; dd++)
                        if (hm[dd] >= max)
                        {
                            FORC3 avg[c] += rgb[dd][row][col][c];
                            avg[3]++;
                        }
                    FORC3 image[(row + top) * width + col + left][c] = avg[c] / avg[3];
                }
        }
    }
  
#ifdef LIBRAW_USE_OPENMP
#pragma omp barrier
#endif

    free_omp_buffers(buffers, buffer_count);
#ifdef LIBRAW_XTRANS_BOXSUM
    free(box);
#endif

    border_interpolate(8);
}
#undef fcol
