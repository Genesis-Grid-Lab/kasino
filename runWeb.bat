@echo off

set target=Targets-web

if exist %target%\ (
    if exist %target%\Debug\ (
        set target=%target%\Debug\bin
    ) else if exist %target%\Release\ (
        set target=%target%\Release\bin
    ) else (
        echo Can't find folder Debug or Release inside %target%
        goto end
    )
) else (
    echo Run build.bat first
    goto end
)

start "" /d "%target%" emrun --no_browser --port 8080 Kasino.html

:end