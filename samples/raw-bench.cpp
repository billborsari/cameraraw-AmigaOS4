/*
 * raw-bench.cpp - per-stage timing and output checksums for cameraraw.library
 *
 * Runs open -> unpack -> dcraw_process -> make_mem_image [-> write] through
 * the library interface (the same path an application uses), timing each
 * step and the stages inside dcraw_process (from LibRaw's progress
 * callbacks), and prints CRC32s of the decoded raw data and the final RGB
 * image so a run on AmigaOS 4 can be compared with a native host build
 * (perf/host) without copying the images off the Amiga.
 *
 * Usage: raw-bench [options] <rawfile>
 *   -q N    interpolation quality (0 linear, 1 VNG, 2 PPG, 3 AHD, 4 DCB, 11 DHT, 12 AAHD)
 *   -h      half-size
 *   -w      camera white balance
 *   -6      16-bit output       -4  linear 16-bit (-6 -W -g 1 1)
 *   -o F    also write PPM/TIFF to F (timed)   -T  write TIFF
 *   -n N    repeat N times (reports each run)
 *   -L lib  library to open (default cameraraw.library)
 *   -v      print every progress callback
 *   -R F    write one CRC per output row to F (to locate differences)
 *   -d F    dump the final RGB samples to F (for diffing)
 *   -m      read the file into memory first and use CameraRaw_OpenBuffer
 *   -c      CRC imgdata.image at the end of each stage (slow; for finding
 *           where host and Amiga output first differ)
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR CDDL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <proto/exec.h>
#include "libraw/libraw.h"
#include "../cameraraw.library/cameraraw.h"

#ifdef __amigaos4__
static const char __attribute__((used)) stack_cookie[] = "$STACK:2000000";
#endif

static struct CameraRawIFace *ICR;

static double now_ms(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static unsigned long avail_mem(void)
{
#ifdef __amigaos4__
  return IExec->AvailMem(MEMF_ANY);
#else
  return 0;
#endif
}

/* CRC-32 (IEEE), same polynomial as zlib, so host tools can cross-check. */
static unsigned crc_table[256];
static void crc_init(void)
{
  for (unsigned n = 0; n < 256; n++)
  {
    unsigned c = n;
    for (int k = 0; k < 8; k++)
      c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    crc_table[n] = c;
  }
}
static unsigned crc32_buf(const void *p, size_t len)
{
  const unsigned char *b = (const unsigned char *)p;
  unsigned c = 0xFFFFFFFFu;
  while (len--)
    c = crc_table[(c ^ *b++) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}
/* CRC of an array of 16-bit values taken little-endian, so a big-endian
 * Amiga and a little-endian host agree when the values agree. */
static unsigned crc32_u16le(const unsigned short *p, size_t n)
{
  unsigned c = 0xFFFFFFFFu;
  while (n--)
  {
    unsigned v = *p++;
    c = crc_table[(c ^ v) & 0xFF] ^ (c >> 8);
    c = crc_table[(c ^ (v >> 8)) & 0xFF] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFu;
}
static LibRaw *g_raw;
static int stage_crc;

/* Stage timeline from progress callbacks. */
#define MAXEV 256
static struct ev { int stage, iter, expected; double t; unsigned long freemem; } evs[MAXEV];
static int nev;
static int verbose;
static unsigned long min_free;
static double t_base;

static int progress_cb(void *, enum LibRaw_progress p, int iter, int expected)
{
  double t = now_ms();
  unsigned long fm = avail_mem();
  if (fm && fm < min_free)
    min_free = fm;
  if (verbose)
    printf("    cb %-28s %d/%d  %9.1f ms\n", ICR->CameraRaw_StrProgress(ICR, p), iter, expected, t - t_base);
  if (stage_crc && g_raw && iter == expected - 1 && g_raw->imgdata.image)
  {
    libraw_image_sizes_t &sz = g_raw->imgdata.sizes;
    size_t n = (size_t)sz.iheight * sz.iwidth * 4;
    printf("    crc after %-24s %08x\n", ICR->CameraRaw_StrProgress(ICR, p),
           crc32_u16le((unsigned short *)g_raw->imgdata.image, n));
    t = now_ms(); /* keep the CRC out of the stage times */
  }
  if (nev < MAXEV)
  {
    evs[nev].stage = p; evs[nev].iter = iter; evs[nev].expected = expected;
    evs[nev].t = t; evs[nev].freemem = fm;
    nev++;
  }
  return 0;
}

static void note_mem(void)
{
  unsigned long fm = avail_mem();
  if (fm && fm < min_free)
    min_free = fm;
}

/* Collapse the callback timeline into [first call, last call] per stage,
 * in order of first appearance, plus the gaps between stages. */
static void print_stages(double t0, double t1)
{
  int seen[64]; int nseen = 0;
  double first[64], last[64]; int stage[64];
  for (int i = 0; i < nev; i++)
  {
    int j;
    for (j = 0; j < nseen; j++)
      if (stage[j] == evs[i].stage) break;
    if (j == nseen)
    {
      if (nseen == 64) continue;
      stage[nseen] = evs[i].stage; first[nseen] = last[nseen] = evs[i].t; seen[nseen] = 1; nseen++;
    }
    else
      last[j] = evs[i].t;
  }
  double prev = t0;
  for (int j = 0; j < nseen; j++)
  {
    if (first[j] - prev >= 0.05)
      printf("    %-34s %10.1f ms\n", "(between callbacks)", first[j] - prev);
    printf("    %-34s %10.1f ms\n", ICR->CameraRaw_StrProgress(ICR, stage[j]), last[j] - first[j]);
    prev = last[j];
  }
  if (t1 - prev >= 0.05)
    printf("    %-34s %10.1f ms\n", "(after last callback)", t1 - prev);
  (void)seen;
}

int main(int argc, char **argv)
{
  int use_mem = 0;
  const char *dumpfile = NULL, *rowcrcfile = NULL;
  int quality = -1, half = 0, camwb = 0, out16 = 0, linear = 0, tiff = 0, repeat = 1;
  const char *outfile = NULL, *libname = "cameraraw.library", *file = NULL;

  for (int i = 1; i < argc; i++)
  {
    if (!strcmp(argv[i], "-q") && i + 1 < argc) quality = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-h")) half = 1;
    else if (!strcmp(argv[i], "-w")) camwb = 1;
    else if (!strcmp(argv[i], "-6")) out16 = 1;
    else if (!strcmp(argv[i], "-4")) linear = 1;
    else if (!strcmp(argv[i], "-T")) tiff = 1;
    else if (!strcmp(argv[i], "-v")) verbose = 1;
    else if (!strcmp(argv[i], "-c")) stage_crc = 1;
    else if (!strcmp(argv[i], "-m")) use_mem = 1;
    else if (!strcmp(argv[i], "-d") && i + 1 < argc) dumpfile = argv[++i];
    else if (!strcmp(argv[i], "-R") && i + 1 < argc) rowcrcfile = argv[++i];
    else if (!strcmp(argv[i], "-o") && i + 1 < argc) outfile = argv[++i];
    else if (!strcmp(argv[i], "-n") && i + 1 < argc) repeat = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-L") && i + 1 < argc) libname = argv[++i];
    else if (argv[i][0] == '-') { fprintf(stderr, "unknown option %s\n", argv[i]); return 1; }
    else file = argv[i];
  }
  if (!file)
  {
    printf("Usage: %s [-q N] [-h] [-w] [-6] [-4] [-T] [-o out] [-n N] [-L lib] [-v] rawfile\n", argv[0]);
    return 1;
  }
  crc_init();

  struct Library *base = IExec->OpenLibrary(libname, 0);
  if (!base) { fprintf(stderr, "cannot open %s\n", libname); return 1; }
  ICR = (struct CameraRawIFace *)IExec->GetInterface(base, "main", 1, NULL);
  if (!ICR) { IExec->CloseLibrary(base); return 1; }
  printf("library %s %d.%d [%s]\n", libname, (int)base->lib_Version, (int)base->lib_Revision,
         base->lib_IdString ? (const char *)base->lib_IdString : "");

  int rc = 0;
  for (int run = 0; run < repeat; run++)
  {
    unsigned long free0 = avail_mem();
    min_free = free0 ? free0 : ~0UL;
    nev = 0;
    double ta = now_ms();
    APTR h = ICR->CameraRaw_Init(ICR, 0);
    if (!h) { rc = 1; break; }
    LibRaw *raw = (LibRaw *)h;
    g_raw = raw;
    if (quality >= 0) raw->imgdata.params.user_qual = quality;
    if (half) raw->imgdata.params.half_size = 1;
    if (camwb) raw->imgdata.params.use_camera_wb = 1;
    if (out16) raw->imgdata.params.output_bps = 16;
    if (linear)
    {
      raw->imgdata.params.output_bps = 16;
      raw->imgdata.params.no_auto_bright = 1;
      raw->imgdata.params.gamm[0] = raw->imgdata.params.gamm[1] = 1.0;
    }
    if (tiff) raw->imgdata.params.output_tiff = 1;
    ICR->CameraRaw_SetProgressHandler(ICR, h, (APTR)progress_cb, NULL);

    double t0 = now_ms();
    int r;
    void *membuf = NULL;
    if (use_mem)
    {
      /* read the whole file with one fread, then open_buffer() */
      FILE *fp = fopen(file, "rb");
      long fsz = 0;
      if (fp && !fseek(fp, 0, SEEK_END) && (fsz = ftell(fp)) > 0 && !fseek(fp, 0, SEEK_SET) &&
          (membuf = malloc(fsz)) && fread(membuf, 1, fsz, fp) == (size_t)fsz)
      {
        double tr = now_ms();
        printf("  read file to memory %10.1f ms (%ld bytes)\n", tr - t0, fsz);
        t0 = tr;
        r = ICR->CameraRaw_OpenBuffer(ICR, h, membuf, (uint32)fsz);
      }
      else
        r = LIBRAW_IO_ERROR;
      if (fp) fclose(fp);
    }
    else
      r = ICR->CameraRaw_OpenFile(ICR, h, file);
    double t1 = now_ms();
    if (r) { printf("open failed: %s\n", ICR->CameraRaw_StrError(ICR, r)); rc = 1; ICR->CameraRaw_Close(ICR, h); break; }
    note_mem();
    nev = 0;
    t_base = now_ms();
    r = ICR->CameraRaw_Unpack(ICR, h);
    double t2 = now_ms();
    if (r) { printf("unpack failed: %s\n", ICR->CameraRaw_StrError(ICR, r)); rc = 1; ICR->CameraRaw_Close(ICR, h); break; }
    note_mem();

    libraw_rawdata_t &rd = raw->imgdata.rawdata;
    unsigned raw_crc = 0;
    size_t raw_bytes = 0;
    if (rd.raw_alloc)
    {
      raw_bytes = (size_t)rd.sizes.raw_height * rd.sizes.raw_pitch;
      if (rd.raw_image || rd.color3_image || rd.color4_image)
        raw_crc = crc32_u16le((unsigned short *)rd.raw_alloc, raw_bytes / 2);
      else
        raw_crc = crc32_buf(rd.raw_alloc, raw_bytes); /* float data: not endian-neutral */
    }

    nev = 0;
    double t3 = now_ms();
    t_base = t3;
    r = ICR->CameraRaw_Process(ICR, h);
    double t4 = now_ms();
    if (r) { printf("process failed: %s\n", ICR->CameraRaw_StrError(ICR, r)); rc = 1; ICR->CameraRaw_Close(ICR, h); break; }
    note_mem();
    int nproc = nev;
    struct ev saved[MAXEV];
    memcpy(saved, evs, sizeof(saved[0]) * nproc);

    int err = 0;
    double t5 = now_ms();
    libraw_processed_image_t *img = (libraw_processed_image_t *)ICR->CameraRaw_MakeMemImage(ICR, h, &err);
    double t6 = now_ms();
    note_mem();
    unsigned img_crc = 0;
    if (img && dumpfile)
    {
      /* raw RGB samples (16-bit ones in host order), for diffing */
      FILE *df = fopen(dumpfile, "wb");
      if (df)
      {
        fwrite(img->data, 1, img->data_size, df);
        fclose(df);
      }
    }
    if (img && rowcrcfile)
    {
      /* one CRC per image row: locates differences without copying the image */
      FILE *rf = fopen(rowcrcfile, "w");
      if (rf)
      {
        size_t rowbytes = (size_t)img->width * img->colors * (img->bits / 8);
        for (int y = 0; y < img->height; y++)
        {
          const unsigned char *rp = img->data + y * rowbytes;
          fprintf(rf, "%08x\n", img->bits == 16 ? crc32_u16le((const unsigned short *)rp, rowbytes / 2)
                                                 : crc32_buf(rp, rowbytes));
        }
        fclose(rf);
      }
    }
    if (img)
      img_crc = img->bits == 16 ? crc32_u16le((unsigned short *)img->data, img->data_size / 2)
                                : crc32_buf(img->data, img->data_size);

    double t7 = 0, t8 = 0;
    if (outfile)
    {
      t7 = now_ms();
      r = ICR->CameraRaw_PpmTiffWriter(ICR, h, outfile);
      t8 = now_ms();
      if (r) printf("write failed: %s\n", ICR->CameraRaw_StrError(ICR, r));
    }

    printf("file %s  camera %s %s  raw %dx%d  out %dx%dx%d/%d\n", file,
           ICR->CameraRaw_GetMake(ICR, h), ICR->CameraRaw_GetModel(ICR, h),
           raw->imgdata.sizes.raw_width, raw->imgdata.sizes.raw_height,
           img ? img->width : 0, img ? img->height : 0, img ? img->colors : 0, img ? img->bits : 0);
    printf("decoder %s  filters %08x  q %d half %d\n", raw->imgdata.idata.filters == 9 ? "X-Trans" : "",
           raw->imgdata.idata.filters, quality, half);
    printf("run %d\n", run + 1);
    printf("  open/identify       %10.1f ms\n", t1 - t0);
    printf("  unpack (decode)     %10.1f ms\n", t2 - t1);
    printf("  dcraw_process       %10.1f ms\n", t4 - t3);
    nev = nproc;
    memcpy(evs, saved, sizeof(saved[0]) * nproc);
    print_stages(t3, t4);
    printf("  make_mem_image      %10.1f ms\n", t6 - t5);
    if (outfile)
      printf("  write %-13s %10.1f ms\n", tiff ? "TIFF" : "PPM", t8 - t7);
    printf("  total               %10.1f ms\n", (outfile ? t8 : t6) - t0);
    printf("  crc raw %08x (%lu bytes)  image %08x (%lu bytes)\n", raw_crc, (unsigned long)raw_bytes, img_crc,
           img ? (unsigned long)img->data_size : 0UL);
    if (free0)
      printf("  peak mem (sampled)  %10.1f MB\n", (free0 - min_free) / 1048576.0);
    (void)ta;

    if (img)
      ICR->CameraRaw_FreeMemImage(ICR, img);
    ICR->CameraRaw_Close(ICR, h);
    free(membuf);
  }

  IExec->DropInterface((struct Interface *)ICR);
  IExec->CloseLibrary(base);
  return rc;
}
