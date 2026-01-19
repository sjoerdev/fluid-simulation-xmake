@echo off

CALL "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\vc\Auxiliary\Build\vcvarsall.bat" x64

cmake -S . -B build -G Ninja
cmake --build build --config Debug --parallel