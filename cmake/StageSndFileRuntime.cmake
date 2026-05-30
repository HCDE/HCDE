set(_hcde_sndfile_candidates "")

if(DEFINED ENV{HCDE_SNDFILE_RUNTIME_DLL} AND NOT "$ENV{HCDE_SNDFILE_RUNTIME_DLL}" STREQUAL "")
	list(APPEND _hcde_sndfile_candidates "$ENV{HCDE_SNDFILE_RUNTIME_DLL}")
endif()

if(DEFINED ENV{HCDE_BINARY_DIR} AND NOT "$ENV{HCDE_BINARY_DIR}" STREQUAL "")
	list(APPEND _hcde_sndfile_candidates
		"$ENV{HCDE_BINARY_DIR}/deps/sndfile.dll"
		"$ENV{HCDE_BINARY_DIR}/sndfile.dll"
	)
endif()

set(_hcde_sndfile_source "")
foreach(_hcde_candidate IN LISTS _hcde_sndfile_candidates)
	if(NOT _hcde_candidate STREQUAL "" AND EXISTS "${_hcde_candidate}")
		set(_hcde_sndfile_source "${_hcde_candidate}")
		break()
	endif()
endforeach()

if(_hcde_sndfile_source STREQUAL "")
	message(WARNING "sndfile.dll not staged: place it in build/deps, build/, or set HCDE_SNDFILE_RUNTIME_DLL. OGG/FLAC/WAV music will not decode without it.")
	return()
endif()

set(_hcde_sndfile_destinations "")
if(DEFINED ENV{HCDE_TARGET_DIR} AND NOT "$ENV{HCDE_TARGET_DIR}" STREQUAL "")
	list(APPEND _hcde_sndfile_destinations "$ENV{HCDE_TARGET_DIR}")
endif()
if(DEFINED ENV{HCDE_RESOURCE_DIR} AND NOT "$ENV{HCDE_RESOURCE_DIR}" STREQUAL "")
	list(APPEND _hcde_sndfile_destinations "$ENV{HCDE_RESOURCE_DIR}")
endif()
list(REMOVE_DUPLICATES _hcde_sndfile_destinations)

foreach(_hcde_dest IN LISTS _hcde_sndfile_destinations)
	file(MAKE_DIRECTORY "${_hcde_dest}")
	file(COPY_FILE "${_hcde_sndfile_source}" "${_hcde_dest}/sndfile.dll" ONLY_IF_DIFFERENT)
endforeach()

get_filename_component(_hcde_sndfile_dir "${_hcde_sndfile_source}" DIRECTORY)
foreach(_hcde_license_name IN ITEMS SNDFILE-LICENSE.txt lgpl.txt COPYING.LIB COPYING.LESSER)
	if(EXISTS "${_hcde_sndfile_dir}/${_hcde_license_name}")
		foreach(_hcde_dest IN LISTS _hcde_sndfile_destinations)
			file(COPY_FILE "${_hcde_sndfile_dir}/${_hcde_license_name}" "${_hcde_dest}/SNDFILE-LICENSE.txt" ONLY_IF_DIFFERENT)
		endforeach()
		break()
	endif()
endforeach()
