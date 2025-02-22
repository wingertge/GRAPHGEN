# Copyright (c) 2020, the GRAPHGEN contributors, as
# shown by the AUTHORS file. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

cmake_minimum_required (VERSION 3.13)

set (ProjectName "GRAPHGEN")
project (${ProjectName})

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configs" FORCE)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE PATH "Install prefix" FORCE)

#Set GRAPHGEN variables
set(GRAPHGEN_OUTPUT_PATH "${CMAKE_INSTALL_PREFIX}/outputs" CACHE PATH "'output' results path")
set(GRAPHGEN_INPUT_PATH "${CMAKE_INSTALL_PREFIX}/input" CACHE PATH "'input' dataset path")

set(GRAPHGEN_FREQUENCIES_ENABLED OFF CACHE BOOL "Enable Frequency Calculation in Connected Components Labeling.")

if(GRAPHGEN_FREQUENCIES_ENABLED)
	set(GRAPHGEN_FREQUENCIES_DOWNLOAD_DATASET OFF CACHE BOOL "Enable automatic download of the dataset used for frequency calculation.")
    add_compile_definitions(GRAPHGEN_FREQUENCIES_ENABLED)
endif()


if(MSVC)
  set(CMAKE_USE_RELATIVE_PATHS ON CACHE INTERNAL "" FORCE)
endif()

# ----------------------------------------------------------------------------
# Solution folders
# ----------------------------------------------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

# --------------------
# OpenCV 
# --------------------
if(GRAPHGEN_FREQUENCIES_ENABLED)
	set(OpenCV_REQUIRED_PACKAGES "core;imgcodecs;imgproc" CACHE STRING "OpenCV packages required by GRAPHGEN are already set")
	FIND_PACKAGE(OpenCV REQUIRED ${OpenCV_REQUIRED_PACKAGES})
	include_directories( ${OpenCV_INCLUDE_DIRS} )
	if(MSVC)
		if("${OpenCV_STATIC}" STREQUAL "ON" OR "${OpenCV_SHARED}" STREQUAL "OFF")
			message(STATUS "Setting static runtime for MSVC")
			foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO) 
				string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
			endforeach(flag_var)
		endif()
	endif()
endif()

find_package(Git QUIET)

# --------------------
# 3rdparty 
# --------------------
set(CMAKE_FOLDER 3rdparty)
add_subdirectory(3rdparty)
set(CMAKE_FOLDER "")
# --------------------

# --------------------
# Project
# --------------------
add_library(GRAPHGEN STATIC "${CMAKE_INSTALL_PREFIX}/config.yaml")
target_include_directories(GRAPHGEN PUBLIC src/GRAPHGEN)
add_subdirectory(src/GRAPHGEN)
target_link_libraries(GRAPHGEN yaml-cpp)

if (WIN32)
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,/LARGEADDRESSAWARE")
endif (WIN32)

set(LABELING_ALGORITHMS SAUF SAUF3D SAUF++3D PRED PRED3D PRED++ PRED++3D BBDT DRAG Spaghetti Spaghetti4C Tagliatelle Spaghetti_CTBE CACHE INTERNAL ON FORCE)
if(GRAPHGEN_FREQUENCIES_ENABLED)
	set(LABELING_ALGORITHMS ${LABELING_ALGORITHMS} DRAG_FREQ BBDT_FREQ Spaghetti_FREQ)
endif()

foreach(ALGO ${LABELING_ALGORITHMS})
	add_executable(${ALGO} "")
	set_target_properties(${ALGO} PROPERTIES FOLDER "Labeling")
	include_directories(src/Labeling)
	add_subdirectory(src/Labeling/${ALGO})
	target_link_libraries (${ALGO} GRAPHGEN)
endforeach()

set(THINNING_ALGORITHMS ZS_Tree ZS_Spaghetti GH_Tree GH_Spaghetti CH_Tree CH_Spaghetti CACHE INTERNAL ON FORCE)
if(GRAPHGEN_FREQUENCIES_ENABLED)
	set(THINNING_ALGORITHMS ${THINNING_ALGORITHMS} ZS_Spaghetti_FREQ GH_Spaghetti_FREQ CH_Spaghetti_FREQ)
endif()

foreach(ALGO ${THINNING_ALGORITHMS})
	add_executable(${ALGO} "")
	set_target_properties(${ALGO} PROPERTIES FOLDER "Thinning")
	include_directories(src/Thinning)
	add_subdirectory(src/Thinning/${ALGO})
	target_link_libraries (${ALGO} GRAPHGEN)
endforeach()

set(CHAINCODE_ALGORITHMS Cederberg_Tree Cederberg_Spaghetti CACHE INTERNAL ON FORCE)
if(GRAPHGEN_FREQUENCIES_ENABLED)
	set(CHAINCODE_ALGORITHMS ${CHAINCODE_ALGORITHMS} Cederberg_Spaghetti_FREQ)
endif()

foreach(ALGO ${CHAINCODE_ALGORITHMS})
	add_executable(${ALGO} "")
	set_target_properties(${ALGO} PROPERTIES FOLDER "ChainCode")
	include_directories(src/ChainCode)
	add_subdirectory(src/ChainCode/${ALGO})
	target_link_libraries (${ALGO} GRAPHGEN)
endforeach()

set(MORPHOLOGY_ALGORITHMS Erosion_Tree Erosion_Spaghetti Dilation_Tree Dilation_Spaghetti CACHE INTERNAL ON FORCE)
if(GRAPHGEN_FREQUENCIES_ENABLED)
	set(MORPHOLOGY_ALGORITHMS ${MORPHOLOGY_ALGORITHMS} Erosion_Spaghetti_FREQ Dilation_Spaghetti_FREQ)
endif()

foreach(ALGO ${MORPHOLOGY_ALGORITHMS})
	add_executable(${ALGO} "")
	set_target_properties(${ALGO} PROPERTIES FOLDER "Morphology")
	include_directories(src/Morphology)
	add_subdirectory(src/Morphology/${ALGO})
	target_link_libraries (${ALGO} GRAPHGEN)
endforeach()

# Check for c++23 support (TODO check if it actually works)
set_property(TARGET ${LABELING_ALGORITHMS} ${THINNING_ALGORITHMS} ${CHAINCODE_ALGORITHMS} ${MORPHOLOGY_ALGORITHMS} GRAPHGEN PROPERTY CXX_STANDARD 23)
set_property(TARGET ${LABELING_ALGORITHMS} ${THINNING_ALGORITHMS} ${CHAINCODE_ALGORITHMS} ${MORPHOLOGY_ALGORITHMS} GRAPHGEN PROPERTY CXX_STANDARD_REQUIRED ON)

#add_definitions(-D_CRT_SECURE_NO_WARNINGS) #To suppress 'fopen' opencv warning/bug  
# Set configuration file	
configure_file("${CMAKE_SOURCE_DIR}/CMake.config.yaml" "${CMAKE_INSTALL_PREFIX}/config.yaml")

# Copy frequencies files into bin folder
file(COPY "${CMAKE_SOURCE_DIR}/frequencies" DESTINATION "${GRAPHGEN_OUTPUT_PATH}/")

# Copy rule_sets into output folder
file(COPY "${CMAKE_SOURCE_DIR}/rulesets/ChainCode_rstable.yaml" DESTINATION "${GRAPHGEN_OUTPUT_PATH}/")
file(COPY "${CMAKE_SOURCE_DIR}/rulesets/ctbe_rstable.yaml" DESTINATION "${GRAPHGEN_OUTPUT_PATH}/")

#set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${ProjectName}) # Set GRAPHGEN as startup group instead of ALL_BUILD
if(GRAPHGEN_FREQUENCIES_ENABLED)
	target_link_libraries (${ProjectName} ${OpenCV_LIBS})
endif()

# Creation of output directory
if(GRAPHGEN_FREQUENCIES_DOWNLOAD_DATASET)
	# Creation of the input directory
	if (NOT EXISTS "${GRAPHGEN_INPUT_PATH}")
		file(MAKE_DIRECTORY ${GRAPHGEN_INPUT_PATH})
	endif()
	
	if (EXISTS "${GRAPHGEN_INPUT_PATH}/input.zip")
		message(STATUS "${GRAPHGEN_INPUT_PATH}/input.zip already exists, download skipped!")
	else()
		message(STATUS "Downloading dataset...")
		file(DOWNLOAD https://www.dropbox.com/s/0exhktek8txha9c/input.zip?dl=1 "${GRAPHGEN_INPUT_PATH}/input.zip" STATUS status SHOW_PROGRESS)
		list( GET status 0 error_code )
		if( error_code )
			message(ERROR " Cmake is not able to download the GRAPHGEN dataset. \nPlease, retry or download it manually and put all unzipped dataset directories in the 'input' path:\n'" ${GRAPHGEN_INPUT_PATH} "'. \n(The dataset is available at https://www.dropbox.com/s/0exhktek8txha9c/input.zip?dl=1).")
			if(EXISTS "${GRAPHGEN_INPUT_PATH}/input.zip")
				file(REMOVE "${GRAPHGEN_INPUT_PATH}/input.zip")
			endif()
		else()
			message(STATUS "Downloading done.")
		endif()
		
		if(EXISTS "${GRAPHGEN_INPUT_PATH}/input.zip")
			message(STATUS "Unpacking (it may take a while)...")
			execute_process(COMMAND ${CMAKE_COMMAND} -E tar xvf "${GRAPHGEN_INPUT_PATH}/input.zip" WORKING_DIRECTORY "${GRAPHGEN_INPUT_PATH}" OUTPUT_QUIET)
			message(STATUS "Unpacking done.")
		endif()
	endif()
endif()

if (NOT EXISTS "${GRAPHGEN_OUTPUT_PATH}")
	file(MAKE_DIRECTORY ${GRAPHGEN_OUTPUT_PATH})
endif()

