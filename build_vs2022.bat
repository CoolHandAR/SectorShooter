@echo off

pushd %1
premake5.exe vs2022
popd
pause