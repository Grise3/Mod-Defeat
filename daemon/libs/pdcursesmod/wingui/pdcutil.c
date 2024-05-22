/* Public Domain Curses */

#include "pdcwin.h"
#ifdef WIN32_LEAN_AND_MEAN
#include <mmsystem.h>
#include <process.h>
#endif

static volatile int _beep_count = 0;

static void beep_thread(LPVOID lpParameter)
{
    INTENTIONALLY_UNUSED_PARAMETER( lpParameter);
    while( _beep_count)
    {
        if (!PlaySound((LPCTSTR) SND_ALIAS_SYSTEMDEFAULT, NULL, SND_ALIAS_ID))
            Beep(800, 200);
        _beep_count--;
    }
}

void PDC_beep(void)
{
    PDC_LOG(("PDC_beep() - called\n"));
    _beep_count++;
    if( _beep_count == 1)
        _beginthread( beep_thread, 0, NULL);
}

/* This used to call Sleep() hence "nap". Sleep calls have been removed
   so as not to inhibit Daemon's performance. There is no real
   need to sleep; rather this is abused to pump the event loop */
void PDC_napms(int ms)     /* 'ms' = milli,  _not_ microseconds! */
{
    INTENTIONALLY_UNUSED_PARAMETER( ms);
    /* RR: keep GUI window responsive while PDCurses sleeps */
    MSG msg;

    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    /* Pump all pending messages from WIN32 to the window handler */
    while( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

const char *PDC_sysname(void)
{
   return "WinGUI";
}

enum PDC_port PDC_port_val = PDC_PORT_WINGUI;
