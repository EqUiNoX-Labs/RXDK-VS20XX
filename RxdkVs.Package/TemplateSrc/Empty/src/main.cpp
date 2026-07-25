// Minimal RXDK Xbox title — an empty application. Brings up nothing and idles, so the
// title stays resident (returning from main would reboot the console to the dashboard).
// A starting point to add your own initialization.
#include <xtl.h>

void __cdecl main()
{
    for (;;)
    {
        Sleep(100);
    }
}
