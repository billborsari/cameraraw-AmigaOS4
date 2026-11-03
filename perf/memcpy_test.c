/* memcpy_test - check the guest's newlib memcpy/memmove on large blocks.
 * (amiga_mcp CLAUDE.md: a memcpy >= ~300 KB on the QEMU guest can leave
 * its last 256 KB uncopied, depending on alignment.)
 * Build: ppc-amigaos-gcc -O2 -mcrt=newlib memcpy_test.c -o memcpy_test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
  static const size_t sizes[] = {64 << 10, 200 << 10, 300 << 10, 512 << 10, 1536 << 10, 6 << 20};
  size_t maxsz = (6 << 20) + 64;
  unsigned char *a = malloc(maxsz), *b = malloc(maxsz);
  if (!a || !b) { printf("no memory\n"); return 20; }
  int bad = 0, tests = 0;
  for (int fn = 0; fn < 2; fn++)
    for (unsigned si = 0; si < sizeof(sizes) / sizeof(sizes[0]); si++)
      for (int sa = 0; sa < 8; sa += 3)
        for (int da = 0; da < 8; da += 4)
        {
          size_t n = sizes[si];
          for (size_t i = 0; i < n + 16; i++) a[i] = (unsigned char)(i * 7 + si + sa);
          memset(b, 0xEE, n + 16);
          if (fn == 0) memcpy(b + da, a + sa, n); else memmove(b + da, a + sa, n);
          tests++;
          size_t first = (size_t)-1;
          for (size_t i = 0; i < n; i++)
            if (b[da + i] != a[sa + i]) { first = i; break; }
          if (first != (size_t)-1)
          {
            size_t cnt = 0;
            for (size_t i = first; i < n; i++) if (b[da + i] != a[sa + i]) cnt++;
            printf("%s size %lu src+%d dst+%d: first bad byte %lu, %lu bad bytes\n", fn ? "memmove" : "memcpy",
                   (unsigned long)n, sa, da, (unsigned long)first, (unsigned long)cnt);
            bad++;
          }
        }
  printf("memcpy_test: %d of %d copies wrong\n", bad, tests);
  return bad ? 10 : 0;
}
