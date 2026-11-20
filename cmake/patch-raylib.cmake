# Applied to raylib 5.5 by FetchContent (PATCH_COMMAND, cwd = raylib source).
# InitWindow() ignores InitPlatform()'s failure and goes on to rlglInit(),
# which calls null GL function pointers and segfaults when no OpenGL 3.3
# context can be created (no display, VM without GPU, ...). Return early
# instead so IsWindowReady() is false and the app can exit cleanly.
file(READ src/rcore.c content)
set(old "    InitPlatform();\n    //--------------------------------------------------------------\n\n    // Initialize rlgl")
set(new "    if (InitPlatform() != 0) { TRACELOG(LOG_WARNING, \"SYSTEM: Failed to initialize platform\"); return; }\n    //--------------------------------------------------------------\n\n    // Initialize rlgl")
string(FIND "${content}" "${old}" pos)
if(pos GREATER -1)
    string(REPLACE "${old}" "${new}" content "${content}")
    file(WRITE src/rcore.c "${content}")
elseif(NOT content MATCHES "if \\(InitPlatform\\(\\) != 0\\)")
    message(FATAL_ERROR "patch-raylib: InitWindow() code not found; update cmake/patch-raylib.cmake")
endif()
