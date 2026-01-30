# Sector Shooter

## Sector based "Doom/Build like" fps game

![SCREENSHOT](screenshot0.png)
![SCREENSHOT](screenshot1.png)
![SCREENSHOT](screenshot2.png)

## Video
https://www.youtube.com/watch?v=weuWWwH3QYY

## Lightmapper
Offline Lightmapper that precalculates Ambient Occlusion, Direct Light and Global Illumination.

![SCREENSHOT](lightmaps_preview.png)

## Lightgrid
Precalculated Light Grid Points spread across the level. Used for quickly lighting dynamic objects.

![SCREENSHOT](lightgrid_preview.png)

## Multithreaded Software Renderer
Rendering is done on the cpu. Multiple threads are used for rendering. Each thread is assigned a horizontal slice and then it performs bsp traversal, draws walls and sprites.  

## Assets
Assets are not my own, except the music. Most assets are taken from https://www.realm667.com/.

## Building
You need visual studio 2022 to build. You also need glfw3.lib which you can get from https://www.glfw.org/download.html. Create a lib folder and place glfw3.lib in the lib folder,
run build_vs2022.bat, and run the solution. Build in either debug or release. Go to bin/release/ and run the exe. The asset folder should be
copied automatically after building, if it doesn't, copy it manually.

## Use at your own risk
