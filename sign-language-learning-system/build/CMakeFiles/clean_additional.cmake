# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "AppLogoTests_autogen"
  "CMakeFiles\\AppLogoTests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\AppLogoTests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\GestureDisplayTests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\GestureDisplayTests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\GestureLibraryCacheTests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\GestureLibraryCacheTests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\GestureScorerTests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\GestureScorerTests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\LoginSubmitGuardTests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\LoginSubmitGuardTests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\SignLanguageLearningSystem_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\SignLanguageLearningSystem_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\UserManagerTests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\UserManagerTests_autogen.dir\\ParseCache.txt"
  "GestureDisplayTests_autogen"
  "GestureLibraryCacheTests_autogen"
  "GestureScorerTests_autogen"
  "LoginSubmitGuardTests_autogen"
  "SignLanguageLearningSystem_autogen"
  "UserManagerTests_autogen"
  )
endif()
