file(GLOB xclipHeaders *.h)
file(GLOB xclipSources *.cpp)
target_sources(Tahoma2D PUBLIC ${xclipHeaders} ${xclipSources})
