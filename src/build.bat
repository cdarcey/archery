@rem keep environment variables modifications local
@setlocal

@rem make script directory CWD
@pushd %~dp0

@rem modify PATH to find vcvarsall.bat
@set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build;%PATH%

@rem setup environment for MSVC dev tools
@call vcvarsall.bat amd64 > nul

@rem default compilation result
@set PL_RESULT=[1m[92mSuccessful.[0m

@rem create main target output directoy
@if not exist "../out" @mkdir "../out"

@rem cleanup binaries if not hot reloading
@if exist "../out/archery.exe" del "..\out\archery.exe"

@rem run compiler (and linker)
@echo.
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m

@rem call compiler
cl main.c ay_rasterize.c ay_threading_win32.c ay_windowing_win32.c -Fe"../out/archery.exe" -Fo"../out/" -Od -Zi -nologo /WX -I"../dependencies/GLFW/include" -I"../data" -I"../dependencies/stb" -MD -link -LIBPATH:"../dependencies/GLFW/libs" opengl32.lib glfw3.lib user32.lib gdi32.lib shell32.lib -incremental:no

@rem check build status
@set PL_BUILD_STATUS=%ERRORLEVEL%

@rem failed
@if %PL_BUILD_STATUS% NEQ 0 (
    @echo [1m[91mCompilation Failed with error code[0m: %PL_BUILD_STATUS%
    @set PL_RESULT=[1m[91mFailed.[0m
    goto Cleanupcpptest
)

@rem cleanup obj files
:Cleanupcpptest
    @echo [1m[36mCleaning...[0m
    @del "..\out\*.obj"  > nul 2> nul


@rem print results
@echo.
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

@rem return CWD to previous CWD
@popd