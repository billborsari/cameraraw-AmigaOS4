/* tlat - timer.device lateness probe for the QEMU AmigaOS 4 guest.
 * Waits N x 40 ms on UNIT_MICROHZ and reports how late each wake-up is.
 * A healthy guest wakes ~1-2 ms late; a degraded one (needs a QEMU restart
 * before timings mean anything) wakes 50+ ms late.
 * Build: ppc-amigaos-gcc -O2 -mcrt=newlib -D__USE_INLINE__ tlat.c -o tlat -lauto */
#include <stdio.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <devices/timer.h>

int main(int argc, char **argv)
{
  int n = argc > 1 ? atoi(argv[1]) : 50;
  struct MsgPort *port = IExec->AllocSysObject(ASOT_PORT, NULL);
  struct TimeRequest *tr = IExec->AllocSysObjectTags(ASOT_IOREQUEST, ASOIOR_Size, sizeof(struct TimeRequest),
                                                     ASOIOR_ReplyPort, port, TAG_END);
  if (!port || !tr || IExec->OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)tr, 0))
  {
    printf("timer.device open failed\n");
    return 20;
  }
  struct TimerIFace *ITimer = (struct TimerIFace *)IExec->GetInterface((struct Library *)tr->Request.io_Device, "main", 1, NULL);
  double sum = 0, max = 0;
  for (int i = 0; i < n; i++)
  {
    struct TimeVal a, b;
    ITimer->GetUpTime(&a);
    tr->Request.io_Command = TR_ADDREQUEST;
    tr->Time.Seconds = 0;
    tr->Time.Microseconds = 40000;
    IExec->DoIO((struct IORequest *)tr);
    ITimer->GetUpTime(&b);
    double late = (b.Seconds - a.Seconds) * 1000.0 + (b.Microseconds - (double)a.Microseconds) / 1000.0 - 40.0;
    sum += late;
    if (late > max) max = late;
  }
  printf("tlat: %d x 40 ms waits, late avg %.2f ms, max %.2f ms\n", n, sum / n, max);
  IExec->DropInterface((struct Interface *)ITimer);
  IExec->CloseDevice((struct IORequest *)tr);
  IExec->FreeSysObject(ASOT_IOREQUEST, tr);
  IExec->FreeSysObject(ASOT_PORT, port);
  return 0;
}
