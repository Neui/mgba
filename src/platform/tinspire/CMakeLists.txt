enable_language(ASM) # Used to convert assets into objects, for now

set(GENERATE_TNS ON CACHE BOOL "Generate .tns file for TI Nspire with Ndless")
mark_as_advanced(GENERATE_TNS)

list(APPEND CORE_VFS_SRC
	"${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-file.c"
	"${CMAKE_SOURCE_DIR}/src/util/vfs/vfs-dirent.c"
	)
set(CORE_VFS_SRC ${CORE_VFS_SRC} PARENT_SCOPE)

set(OS_DEFINES COLOR_16_BIT COLOR_5_6_5 DISABLE_THREADING FIXED_ROM_BUFFER
  USE_VFS_FILE USE_FILE32API IOAPI_NO_64 TINSPIRE)
set(OS_DEFINES ${OS_DEFINES} PARENT_SCOPE)

list(APPEND GUI_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/gui-font.c
	${CMAKE_CURRENT_SOURCE_DIR}/draw.c
	${CMAKE_CURRENT_SOURCE_DIR}/lcd.c
	${CMAKE_CURRENT_SOURCE_DIR}/keys.c
	)

list(APPEND OS_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/memory.c
	)
source_group("TI Nspire-specific code" FILES ${OS_SRC})

add_executable("${BINARY_NAME}.elf" ${GUI_SRC} ${OS_SRC}
	keys.c main.c binaries.s)
set_target_properties("${BINARY_NAME}.elf" PROPERTIES COMPILE_DEFINITIONS "${OS_DEFINES};${FEATURE_DEFINES};${FUNCTION_DEFINES}")
target_link_libraries("${BINARY_NAME}.elf" ${BINARY_NAME} ${M_LIBRARY} ${OS_LIB})

if(GENERATE_TNS)
	set(TNS_APP_VERSION ${LIB_VERSION_MAJOR}${LIB_VERSION_MINOR}${LIB_VERSION_PATCH})
	find_program(GENZEHN genzehn${extension})
	if (NOT GENZEHN)
		message(FATAL_ERROR "Couldn't find genzehn! "
			"Specify using -DGENZEHN=path")
	endif()
	add_custom_command(OUTPUT "${BINARY_NAME}.tns"
			COMMAND "${GENZEHN}"
				--input "${BINARY_NAME}.elf"
				--output "${BINARY_NAME}.tns"
				--name "${PROJECT_NAME}"
				--version "${TNS_APP_VERSION}"
				--compress
			DEPENDS "${BINARY_NAME}.elf"
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
			COMMENT "Building ${BINARY_NAME}.tns")
	add_custom_target("tns" ALL DEPENDS "${BINARY_NAME}.tns")
endif()

install(TARGETS ${BINARY_NAME}.elf DESTINATION . COMPONENT ${BINARY_NAME}-dbg)
if(GENERATE_TNS)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.tns DESTINATION . COMPONENT ${BINARY_NAME}-nspire)
endif()

