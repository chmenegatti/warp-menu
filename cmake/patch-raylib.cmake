# Applied to raylib 5.5 by FetchContent (PATCH_COMMAND, cwd = raylib source).
# Two fixes for crashes when no OpenGL window can be created.
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

# Same failure one level down: when glfwCreateWindow() returns NULL (e.g.
# "NSGL: Failed to find a suitable pixel format"), InitPlatform() calls
# GetCurrentMonitor() -> glfwGetWindowPos(NULL) before checking the handle.
file(READ src/platforms/rcore_desktop_glfw.c content)
set(old "(CORE.Window.title != 0)? CORE.Window.title : \" \", NULL, NULL);\n\n        // After the window was created")
set(new "(CORE.Window.title != 0)? CORE.Window.title : \" \", NULL, NULL);\n        if (!platform.handle) { glfwTerminate(); TRACELOG(LOG_WARNING, \"GLFW: Failed to initialize Window\"); return -1; }\n\n        // After the window was created")
string(FIND "${content}" "${old}" pos)
if(pos GREATER -1)
    string(REPLACE "${old}" "${new}" content "${content}")
    file(WRITE src/platforms/rcore_desktop_glfw.c "${content}")
elseif(NOT content MATCHES "NULL, NULL\\);\n        if \\(!platform.handle\\)")
    message(FATAL_ERROR "patch-raylib: glfwCreateWindow() code not found; update cmake/patch-raylib.cmake")
endif()
