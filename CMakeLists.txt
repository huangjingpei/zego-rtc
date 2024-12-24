cmake_minimum_required(VERSION 3.28...3.30)

add_library(zego-rtc STATIC)
add_library(OBS::zego-rtc ALIAS zego-rtc)

# 使用通配符匹配所有源文件和头文件
file(GLOB ZEGO_RTC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB ZEGO_RTC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

message(STATUS "ZEGO_RTC_SOURCES: ${ZEGO_RTC_SOURCES}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX-")

target_sources(
  zego-rtc
  PRIVATE
     ${ZEGO_RTC_SOURCES}
     ${ZEGO_RTC_HEADERS}
)

target_include_directories(
  zego-rtc 
  PRIVATE 
  "${CMAKE_CURRENT_SOURCE_DIR}/ZegoExpress/win/x64/include"
)


target_link_libraries(zego-rtc
  PRIVATE 
  "${CMAKE_CURRENT_SOURCE_DIR}/ZegoExpress/win/x64/ZegoExpressEngine.lib"
)

add_custom_command(
  TARGET zego-rtc POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/ZegoExpress/win/x64/ZegoExpressEngine.dll"
    "${CMAKE_BINARY_DIR}/rundir/$<CONFIG>/bin/64bit/ZegoExpressEngine.dll"
  COMMENT "Copying ZegoExpressEngine.dll to runtime directory."
)

set_target_properties(zego-rtc PROPERTIES FOLDER deps)
