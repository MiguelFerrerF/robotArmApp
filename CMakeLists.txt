cmake_minimum_required(VERSION 3.16)

project(robotArmApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Qt ---
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core Gui SerialPort Multimedia)
set(QT_VERSION_MAJOR 6)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core Gui SerialPort Multimedia)

# --- OpenCV ---
find_package(OpenCV REQUIRED)
message(STATUS "Found OpenCV: ${OpenCV_VERSION}")
message(STATUS "Include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "Libraries: ${OpenCV_LIBS}")

# --- Fuentes del proyecto ---
set(PROJECT_SOURCES
    main.cpp

    mainwindow.ui
    mainwindow.cpp
    mainwindow.h

    library-serial/SerialPortHandler.h
    library-serial/SerialPortHandler.cpp
    library-serial/SerialConnectionSetupDialog.ui
    library-serial/SerialConnectionSetupDialog.h
    library-serial/SerialConnectionSetupDialog.cpp
    library-serial/SerialMonitorDialog.ui
    library-serial/SerialMonitorDialog.h
    library-serial/SerialMonitorDialog.cpp

    resources.qrc

    library-log/LogHandler.h
    library-log/LogHandler.cpp

    library-robot/RobotConfig.h
    library-robot/RobotHandler.h
    library-robot/RobotHandler.cpp
    library-robot/RobotControlDialog.h
    library-robot/RobotControlDialog.cpp
    library-robot/RobotControlDialog.ui

    library-video/VideoCaptureHandler.h
    library-video/VideoCaptureHandler.cpp
    library-video/VideoManagerDialog.cpp
    library-video/VideoManagerDialog.h
    library-video/VideoManagerDialog.ui
    library-video/VideoCalibrationDialog.cpp
    library-video/VideoCalibrationDialog.h
    library-video/VideoCalibrationDialog.ui
)

# --- Creación del ejecutable ---
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(robotArmApp
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(robotArmApp SHARED ${PROJECT_SOURCES})
    else()
        add_executable(robotArmApp ${PROJECT_SOURCES})
    endif()
endif()

# --- Vincular Qt + OpenCV ---
target_link_libraries(robotArmApp PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt${QT_VERSION_MAJOR}::Multimedia
    ${OpenCV_LIBS}
)
target_include_directories(robotArmApp PRIVATE ${OpenCV_INCLUDE_DIRS})

# --- Copiar recursos ---
add_custom_command(TARGET robotArmApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:robotArmApp>/images"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/images/robot.png"
    "$<TARGET_FILE_DIR:robotArmApp>/images/robot.png"
    COMMENT "Copiando robot.png a la carpeta de imágenes del build"
)

# --- Propiedades específicas para macOS / Windows ---
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.robotArmApp)
endif()

set_target_properties(robotArmApp PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    RC_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/images/icon.ico"
)

# --- Instalación ---
include(GNUInstallDirs)
install(TARGETS robotArmApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(robotArmApp)
endif()
