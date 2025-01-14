@rem keep environment variables modifications local
@setlocal

@rem make script directory CWD
@pushd %~dp0

@rem modify PATH to find vcvarsall.bat
@set PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build;%PATH%
@set PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise/VC\Auxiliary\Build;%PATH%

@rem setup environment for MSVC dev tools
@call vcvarsall.bat amd64 > nul

@rem default compilation result
@set PL_RESULT=[1m[92mSuccessful.[0m

@rem create main target output directoy
@if not exist "./out" @mkdir "./out"

@rem cleanup binaries if not hot reloading
@if exist "./out/main.exe" del ".\out\main.exe"
@if exist "./out/cpptest_*.pdb" del ".\out\cpptest_*.pdb"

@rem create output directory
@if not exist "./out" @mkdir "./out"

@rem run compiler (and linker)
@echo.
@echo [1m[93m~~~~~~~~~~~~~~~~~~~~~~[0m
@echo [1m[36mCompiling and Linking...[0m

@rem call compiler
cl rasterize.c -Fe"./out/main.exe" -Fo"./out/" -Od -Zi -nologo -I"./include" -MD -link -LIBPATH:"./lib" /NODEFAULTLIB:ucrt.lib glfw3.lib opengl32.lib ucrt.lib gdi32.lib Ole32.lib Shell32.lib user32.lib -incremental:no 

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
    @REM @del ".\out\*.obj"  > nul 2> nul


@rem print results
@echo.
@echo [36mResult: [0m %PL_RESULT%
@echo [36m~~~~~~~~~~~~~~~~~~~~~~[0m

@rem return CWD to previous CWD
@popd