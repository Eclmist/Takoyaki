@echo off

echo Generating Visual Studio solutions for Takoyaki
cmake -B build
if %errorlevel% neq 0 (
    echo CMake exited with error code %errorlevel%
    echo Projects generation failed
    color CF
    pause
    exit
)

color 2F
echo All configurations generated successfully
PAUSE

