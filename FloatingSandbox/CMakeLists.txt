
#
# FloatingSandbox application
#

set  (FLOATING_SANDBOX_SOURCES
	AboutDialog.cpp
	AboutDialog.h
	AudioController.h
	CheckForUpdatesDialog.cpp
	CheckForUpdatesDialog.h
	CreditsPanel.cpp
	CreditsPanel.h
	EventTickerPanel.cpp
	EventTickerPanel.h
	GLCanvas.h
	HelpDialog.cpp
	HelpDialog.h
	LocalizationManager.cpp
	LocalizationManager.h
	LoggingDialog.cpp
	LoggingDialog.h
	MainApp.cpp
	MainFrame.cpp
	MainFrame.h
	Music.cpp
	Music.h
	MusicController.cpp
	MusicController.h
	NewVersionDisplayDialog.cpp
	NewVersionDisplayDialog.h
	OpenGLContext.cpp
	OpenGLContext.h
	PreferencesDialog.cpp
	PreferencesDialog.h
	ProbePanel.cpp
	ProbePanel.h
	SettingsDialog.cpp
	SettingsDialog.h
	SettingsManager.cpp
	SettingsManager.h
	ShipDescriptionDialog.cpp
	ShipDescriptionDialog.h
	ShipLoadDialog.cpp
	ShipLoadDialog.h
	ShipPreviewWindow.cpp
	ShipPreviewWindow.h
	SoundController.cpp
	SoundController.h
	Sounds.cpp
	Sounds.h
	SplashScreenDialog.cpp
	SplashScreenDialog.h
	StandardSystemPaths.cpp
	StandardSystemPaths.h
	StartupTipDialog.cpp
	StartupTipDialog.h
	SwitchboardPanel.cpp
	SwitchboardPanel.h
	ToolController.cpp
	ToolController.h
	Tools.cpp
	Tools.h
	UIPreferencesManager.cpp
	UIPreferencesManager.h
	UnhandledExceptionHandler.cpp
	UnhandledExceptionHandler.h
	UpdateChecker.cpp
	UpdateChecker.h
	WxHelpers.cpp
	WxHelpers.h
)

source_group(" " FILES ${FLOATING_SANDBOX_SOURCES})

set(RESOURCE_FILES "")
if (MSVC)
	list(APPEND RESOURCE_FILES
		Resources/FloatingSandbox.rc
		Resources/ShipAAA.ico
		Resources/ShipBBB.ico)
else (MSVC)
	list(APPEND RESOURCE_FILES
		Resources/ShipAAA.xpm
		Resources/ShipBBB.xpm)
endif (MSVC)

source_group("Resources" FILES ${RESOURCE_FILES})

add_executable (FloatingSandbox ${FLOATING_SANDBOX_SOURCES} ${RESOURCE_FILES})

target_include_directories(FloatingSandbox PRIVATE .)
target_include_directories(FloatingSandbox PRIVATE ${wxWidgets_INCLUDE_DIRS})
target_compile_definitions(FloatingSandbox PRIVATE "${wxWidgets_DEFINITIONS}")
target_link_libraries (FloatingSandbox
	GameCoreLib
	GameLib
	GameOpenGLLib
	GPUCalcLib
	UIControlsLib
	sfml-audio
	sfml-network
	sfml-system
	${OPENGL_LIBRARIES}
	${wxWidgets_LIBRARIES}
	${ADDITIONAL_LIBRARIES})


if (MSVC)
	set_target_properties(FloatingSandbox PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /NODEFAULTLIB:MSVCRTD")
endif (MSVC)


#
# VS properties
#

if (MSVC)
	
	set_target_properties(
		FloatingSandbox
		PROPERTIES
			# Set debugger working directory to binary output directory
			VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$(Configuration)"

			# Set output directory to binary output directory - VS will add the configuration type
			RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	)

endif (MSVC)



#
# Copy files
#

message (STATUS "Copying data files and runtime files...")

file(COPY "${CMAKE_SOURCE_DIR}/Data" "${CMAKE_SOURCE_DIR}/Ships"
	DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug")
file(COPY "${CMAKE_SOURCE_DIR}/Data" "${CMAKE_SOURCE_DIR}/Ships"
	DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release")
file(COPY "${CMAKE_SOURCE_DIR}/Data" "${CMAKE_SOURCE_DIR}/Ships"
	DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo")

if (WIN32)
	file(COPY ${DEVIL_RUNTIME_LIBRARIES}
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug")
	file(COPY ${DEVIL_RUNTIME_LIBRARIES}
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release")
	file(COPY ${DEVIL_RUNTIME_LIBRARIES}
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo")

	file(COPY ${SFML_RUNTIME_DEBUG_LIBRARIES}
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug")
	file(COPY ${SFML_RUNTIME_RELEASE_LIBRARIES}
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release")
	file(COPY ${SFML_RUNTIME_RELEASE_LIBRARIES}
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo")
endif (WIN32)


####################################################
# Install
####################################################

# Main binary
install(TARGETS FloatingSandbox
	CONFIGURATIONS Release RelWithDebInfo Debug
        RUNTIME
	DESTINATION .)

# PDB
#TODO: disabled as for now we don't generate PDBs at Release
#if (MSVC)
#	install(FILES $<TARGET_PDB_FILE:FloatingSandbox>
#		CONFIGURATIONS Release
#		DESTINATION .)
#endif (MSVC)

# Runtime libraries
if (WIN32)
	# Devil
	install(FILES ${DEVIL_RUNTIME_LIBRARIES}
		DESTINATION .)
	# SFML
	install(FILES ${SFML_RUNTIME_RELEASE_LIBRARIES}
		DESTINATION .)
endif (WIN32)

# Resources
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Data"
	DESTINATION .)

# Guides
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Guides"
	DESTINATION .)

# Ships
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Ships"
	DESTINATION .
	PATTERN "default_ship.png" EXCLUDE)
install(FILES "${CMAKE_SOURCE_DIR}/Ships/R.M.S. Titanic (With Power).shp"
	DESTINATION Ships
	RENAME "default_ship.shp")

# License et al
install(FILES "${CMAKE_SOURCE_DIR}/license.txt" "${CMAKE_SOURCE_DIR}/changes.txt" "${CMAKE_SOURCE_DIR}/README.md"
	DESTINATION .)
