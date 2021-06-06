cl /Od /MDd /DPLATFORM_DESKTOP /DGRAPHICS_API_OPENGL_33 /DWIN32 /nologo src\main.c /link lib\raylib.lib winmm.lib OpenGL32.lib glu32.lib gdi32.lib User32.lib Shell32.lib Ws2_32.lib /SUBSYSTEM:CONSOLE
cl /Od /EHsc /TC /MDd /DPLATFORM_DESKTOP /DGRAPHICS_API_OPENGL_33 /DWIN32 /nologo src\server.c /link lib\raylib.lib Ws2_32.lib User32.lib Shell32.lib gdi32.lib winmm.lib /SUBSYSTEM:CONSOLE 
