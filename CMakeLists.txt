cmake_minimum_required(VERSION 3.22)
project(TaskManager DESCRIPTION "Manage your tasks and TODOs with this simple application")

include(VelyraBuildUtils/VelyraBuildUtils.cmake)

set(VELYRA_APP_FRAMEWORK_PATH)
vl_include_or_fetch(VelyraAppFramework VELYRA_APP_FRAMEWORK_PATH v0.4)
message(STATUS "VelyraAppFramework found at ${VELYRA_APP_FRAMEWORK_PATH}")

set(TASK_MANAGER_HEADERS
    src/TaskManagerInclude.hpp
    src/TaskManagerLayer.hpp
    src/Layouts/MainLayout.hpp
    src/Model/Date.hpp
    src/Model/Store.hpp
    src/Platform/Autostart.hpp
    src/Popups/ConfirmPopup.hpp
    src/Popups/NamePopup.hpp
    src/Widgets/Widgets.hpp
)

set(TASK_MANAGER_SRC
    src/TaskManagerLayer.cpp
    src/Layouts/MainLayout.cpp
    src/Model/Date.cpp
    src/Model/Store.cpp
    src/Platform/Autostart.cpp
    src/Popups/ConfirmPopup.cpp
    src/Popups/NamePopup.cpp
    src/Widgets/Widgets.cpp
)

add_executable(TaskManager main.cpp ${TASK_MANAGER_SRC} ${TASK_MANAGER_HEADERS})
vl_configure_target(TaskManager)
target_link_libraries(TaskManager PRIVATE VelyraAppFramework)
target_include_directories(TaskManager PRIVATE src/Shared)
target_precompile_headers(TaskManager PRIVATE src/Shared/Pch.hpp)

install(TARGETS TaskManager
    RUNTIME DESTINATION bin
)

