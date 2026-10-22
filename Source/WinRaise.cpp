// Windows only: bring the window frame a plugin view sits in to the front (its own file: <windows.h> defines
// words like "near" / "far" as macros and must not meet the rest of the code)
#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
void rekitRaiseFrame(void* nativeHandle)
{
    auto h = (HWND) nativeHandle;
    if (h == nullptr) return;
    HWND root = GetAncestor(h, GA_ROOT);
    if (root == nullptr || GetForegroundWindow() == root || !IsWindowVisible(root)) return;
    SetWindowPos(root, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(root);
}
#else
void rekitRaiseFrame(void*) {}
#endif
