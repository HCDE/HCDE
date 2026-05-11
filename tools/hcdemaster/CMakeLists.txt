cmake_minimum_required( VERSION 3.16 )

add_executable( hcdemaster hcdemaster.cpp )
target_compile_features( hcdemaster PRIVATE cxx_std_17 )
target_include_directories( hcdemaster PRIVATE ${CMAKE_SOURCE_DIR}/protocol )
set_target_properties( hcdemaster PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${HCDE_OUTPUT_DIR} )

if( WIN32 )
	target_link_libraries( hcdemaster ws2_32 )
endif()

install( TARGETS hcdemaster RUNTIME DESTINATION "." )
