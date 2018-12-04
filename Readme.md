# DirectComposition
Minimal project for demonstrating Win32 and DirectComposition interop

## Requirements
* Windows 10 Creators Update (1703)
* CMake 3.8

### Notes

* No error handling is present
* Code is organised for demonstrating DirectComposition / Direct2D / DWM techniques, not for good structure or re-usability

### Features

* WS_EX_NOREDIRECTIONBITMAP for transparent window
* WS_EX_LAYERED for Win32 HWND DirectComposition Visuals
* DirectComposition and Direct2D for rendering
* DirectComposition and DWM for animation (DWM for cloaking)

For more information, see https://docs.microsoft.com/en-us/windows/desktop/directcomp/directcomposition-portal