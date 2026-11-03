/* Randomised equivalence test: LibRaw_buffered_file_datastream must answer
 * every call exactly as LibRaw_bigfile_datastream (LibRaw's default stream)
 * does. Usage: stream_test <file> [ops] [seed]  -> "OK n ops" or first mismatch. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libraw/libraw.h"

int main(int argc, char **argv)
{
  if (argc < 2) return 2;
  long ops = argc > 2 ? atol(argv[2]) : 200000;
  srand(argc > 3 ? atoi(argv[3]) : 1);
  /* "inside": never seek past the end. AmigaOS newlib's fseeko() refuses
   * that (DOS ChangeFilePosition fails) and leaves the position alone,
   * where POSIX stdio and the buffered stream allow it. */
  bool inside = argc > 4 && !strcmp(argv[4], "inside");
  LibRaw_bigfile_datastream a(argv[1]);
  LibRaw_buffered_file_datastream b(argv[1]);
  if (!a.valid() || !b.valid() || a.size() != b.size()) { printf("open/size mismatch\n"); return 1; }
  INT64 sz = a.size();
  static unsigned char ba[1 << 20], bb[1 << 20];
  for (long i = 0; i < ops; i++)
  {
    int op = rand() % 100;
    const char *what = "";
    bool bad = false;
    if (op < 40) { what = "get_char"; bad = a.get_char() != b.get_char(); }
    else if (op < 55) {
      what = "read";
      size_t n = rand() % 4 == 0 ? rand() % (1 << 20) : rand() % 600;
      size_t el = 1 + rand() % 4;
      int ra = a.read(ba, el, n / el), rb = b.read(bb, el, n / el);
      bad = ra != rb || memcmp(ba, bb, (size_t)ra * el);
    }
    else if (op < 70) {
      what = "seek";
      int w = rand() % 3;
      INT64 o = w == 0 ? (INT64)(rand() % (sz + 1000)) : w == 1 ? (INT64)(rand() % 200000) - 100000 : -(INT64)(rand() % 70000);
      if (w == 1 && a.tell() + o < 0) o = 0;
      if (w == 2 && sz + o < 0) o = 0;
      if (inside)
      {
        INT64 np = w == 0 ? o : w == 1 ? a.tell() + o : sz + o;
        if (np > sz) { w = 0; o = sz; }
      }
      int ra = a.seek(o, w == 0 ? SEEK_SET : w == 1 ? SEEK_CUR : SEEK_END), rb = b.seek(o, w == 0 ? SEEK_SET : w == 1 ? SEEK_CUR : SEEK_END);
      bad = (ra != 0) != (rb != 0);
    }
    else if (op < 85) { what = "tell"; bad = a.tell() != b.tell(); }
    else if (op < 92) { what = "eof"; bad = (a.eof() != 0) != (b.eof() != 0); }
    else if (op < 96) {
      what = "scanf";
      bool isint = rand() & 1;
      int ia = 0, ib = 0; float fa = 0, fb = 0;
      int ra = a.scanf_one(isint ? "%d" : "%f", isint ? (void *)&ia : (void *)&fa);
      int rb = b.scanf_one(isint ? "%d" : "%f", isint ? (void *)&ib : (void *)&fb);
      bad = ra != rb || (ra == 1 && (isint ? ia != ib : fa != fb));
    }
    else {
      what = "gets";
      int n = rand() % 600;
      char sa[700], sb[700];
      memset(sa, 1, sizeof sa); memset(sb, 1, sizeof sb);
      char *ra = a.gets(sa, n), *rb = b.gets(sb, n);
      bad = (ra == 0) != (rb == 0) || (ra && strcmp(sa, sb));
    }
    if (!bad && a.tell() != b.tell()) { what = "position after op"; bad = true; }
    if (bad) { printf("MISMATCH at op %ld (%s), pos %lld vs %lld\n", i, what, (long long)a.tell(), (long long)b.tell()); return 1; }
  }
  printf("OK %ld ops on %s\n", ops, argv[1]);
  return 0;
}
