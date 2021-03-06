# This should be a space separated list of up to three servers which
# will be tried out when testing the util_ping() functionality. The
# list of servers should behave like this:
#
# 1. First server - this should be an existing server which should return true.
# 2. This should be an invalid hostname - should return false.
# 3. This should be an valid host which does not answer ping - i.e currently off?
set(PING_SERVERS "" CACHE STRING  "List of servers for testing ping")


link_directories( ${ERT_BINARY_DIR}/libert_util/src )

add_executable( test_thread_pool test_thread_pool.c )
target_link_libraries( test_thread_pool ert_util test_util )
add_test( test_thread_pool valgrind --error-exitcode=1 --tool=memcheck ${EXECUTABLE_OUTPUT_PATH}/test_thread_pool )


add_executable( ert_util_type_vector_test ert_util_type_vector_test.c )
target_link_libraries( ert_util_type_vector_test ert_util test_util )
add_test( ert_util_type_vector_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_type_vector_test )

add_executable( ert_util_matrix ert_util_matrix.c )
target_link_libraries( ert_util_matrix ert_util test_util )
add_test( ert_util_matrix ${EXECUTABLE_OUTPUT_PATH}/ert_util_matrix )

add_executable( ert_util_subst_list ert_util_subst_list.c )
target_link_libraries( ert_util_subst_list ert_util test_util )
add_test( ert_util_subst_list ${EXECUTABLE_OUTPUT_PATH}/ert_util_subst_list )


add_executable( ert_util_matrix_stat ert_util_matrix_stat.c )
target_link_libraries( ert_util_matrix_stat ert_util test_util )
add_test( ert_util_matrix_stat ${EXECUTABLE_OUTPUT_PATH}/ert_util_matrix_stat )

add_executable( ert_util_buffer ert_util_buffer.c )
target_link_libraries( ert_util_buffer ert_util test_util )
add_test( ert_util_buffer ${EXECUTABLE_OUTPUT_PATH}/ert_util_buffer )

add_executable( ert_util_statistics ert_util_statistics.c )
target_link_libraries( ert_util_statistics ert_util test_util )
add_test( ert_util_statistics ${EXECUTABLE_OUTPUT_PATH}/ert_util_statistics )

add_executable( ert_util_struct_vector ert_util_struct_vector.c )
target_link_libraries( ert_util_struct_vector ert_util test_util )
add_test( ert_util_struct_vector ${EXECUTABLE_OUTPUT_PATH}/ert_util_struct_vector )

add_executable( ert_util_copy_file ert_util_copy_file.c )
target_link_libraries( ert_util_copy_file ert_util test_util )
add_test( ert_util_copy_file ${EXECUTABLE_OUTPUT_PATH}/ert_util_copy_file ${EXECUTABLE_OUTPUT_PATH}/ert_util_copy_file )

add_executable( ert_util_filename ert_util_filename.c )
target_link_libraries( ert_util_filename ert_util test_util )
add_test( ert_util_filename ${EXECUTABLE_OUTPUT_PATH}/ert_util_filename )

add_executable( ert_util_sscan_test ert_util_sscan_test.c )
target_link_libraries( ert_util_sscan_test ert_util test_util )
add_test( ert_util_sscan_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_sscan_test )

add_executable( ert_util_string_util ert_util_string_util.c )
target_link_libraries( ert_util_string_util ert_util test_util )
add_test( ert_util_string_util ${EXECUTABLE_OUTPUT_PATH}/ert_util_string_util )

add_executable( ert_util_vector_test ert_util_vector_test.c )
target_link_libraries( ert_util_vector_test ert_util test_util )
add_test( ert_util_vector_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_vector_test )

add_executable( ert_util_cwd_test ert_util_cwd_test.c )
target_link_libraries( ert_util_cwd_test ert_util test_util )
add_test( ert_util_cwd_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_cwd_test ${CMAKE_CURRENT_BINARY_DIR})

add_executable( ert_util_relpath_test ert_util_relpath_test.c )
target_link_libraries( ert_util_relpath_test ert_util test_util )
add_test( ert_util_relpath_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_relpath_test )

add_executable( ert_util_path_stack_test ert_util_path_stack_test.c )
target_link_libraries( ert_util_path_stack_test ert_util test_util )
add_test( ert_util_path_stack_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_path_stack_test ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

add_executable( ert_util_PATH_test ert_util_PATH_test.c )
target_link_libraries( ert_util_PATH_test ert_util test_util )
add_test( ert_util_PATH_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_PATH_test )

add_executable( ert_util_strcat_test ert_util_strcat_test.c )
target_link_libraries( ert_util_strcat_test ert_util test_util )
add_test( ert_util_strcat_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_strcat_test )

add_executable( ert_util_sprintf_escape ert_util_sprintf_escape.c )
target_link_libraries( ert_util_sprintf_escape ert_util test_util )
add_test( ert_util_sprintf_escape ${EXECUTABLE_OUTPUT_PATH}/ert_util_sprintf_escape )


add_executable( ert_util_stringlist_test ert_util_stringlist_test.c )
target_link_libraries( ert_util_stringlist_test ert_util test_util )
add_test( ert_util_stringlist_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_stringlist_test )

add_executable( ert_util_realpath ert_util_realpath.c )
target_link_libraries( ert_util_realpath ert_util test_util )
add_test( ert_util_realpath ${EXECUTABLE_OUTPUT_PATH}/ert_util_realpath )

add_executable( ert_util_hash_test ert_util_hash_test.c )
target_link_libraries( ert_util_hash_test ert_util test_util )
add_test( ert_util_hash_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_hash_test )

add_executable( ert_util_binary_split ert_util_binary_split.c )
target_link_libraries( ert_util_binary_split ert_util test_util )
add_test( ert_util_binary_split ${EXECUTABLE_OUTPUT_PATH}/ert_util_binary_split )

add_executable( ert_util_logh ert_util_logh.c )
target_link_libraries( ert_util_logh ert_util test_util )
add_test( ert_util_logh ${EXECUTABLE_OUTPUT_PATH}/ert_util_logh )

add_executable( ert_util_rng ert_util_rng.c )
target_link_libraries( ert_util_rng ert_util test_util )
add_test( ert_util_rng ${EXECUTABLE_OUTPUT_PATH}/ert_util_rng )

add_executable( ert_util_time_interval ert_util_time_interval.c )
target_link_libraries( ert_util_time_interval ert_util test_util )
add_test( ert_util_time_interval ${EXECUTABLE_OUTPUT_PATH}/ert_util_time_interval )

add_executable( ert_util_before_after ert_util_before_after.c )
target_link_libraries( ert_util_before_after ert_util test_util )
add_test( ert_util_before_after ${EXECUTABLE_OUTPUT_PATH}/ert_util_before_after )

add_executable( ert_util_approx_equal ert_util_approx_equal.c )
target_link_libraries( ert_util_approx_equal ert_util test_util )
add_test( ert_util_approx_equal ${EXECUTABLE_OUTPUT_PATH}/ert_util_approx_equal )

if (PING_PATH)
   add_executable( ert_util_ping ert_util_ping.c )
   target_link_libraries( ert_util_ping ert_util test_util )
   add_test( ert_util_ping ${EXECUTABLE_OUTPUT_PATH}/ert_util_ping ${PING_SERVERS})
endif()

add_executable( ert_util_file_readable ert_util_file_readable.c )
target_link_libraries( ert_util_file_readable ert_util test_util )
add_test( ert_util_file_readable ${EXECUTABLE_OUTPUT_PATH}/ert_util_file_readable ${FILE_READABLE_SERVERS})

add_executable( ert_util_type_vector_functions ert_util_type_vector_functions.c )
target_link_libraries( ert_util_type_vector_functions ert_util test_util )
add_test( ert_util_type_vector_functions ${EXECUTABLE_OUTPUT_PATH}/ert_util_type_vector_functions)

add_executable( ert_util_addr2line ert_util_addr2line.c )
target_link_libraries( ert_util_addr2line ert_util test_util )
add_test( ert_util_addr2line ${EXECUTABLE_OUTPUT_PATH}/ert_util_addr2line)


add_executable( ert_util_ui_return ert_util_ui_return.c )
target_link_libraries( ert_util_ui_return ert_util test_util )
add_test( ert_util_ui_return  ${EXECUTABLE_OUTPUT_PATH}/ert_util_ui_return )

add_executable( ert_util_clamp ert_util_clamp.c )
target_link_libraries( ert_util_clamp ert_util test_util )
add_test( ert_util_clamp  ${EXECUTABLE_OUTPUT_PATH}/ert_util_clamp )

add_executable( ert_util_strstr_int_format ert_util_strstr_int_format.c )
target_link_libraries( ert_util_strstr_int_format ert_util test_util )
add_test( ert_util_strstr_int_format  ${EXECUTABLE_OUTPUT_PATH}/ert_util_strstr_int_format )

add_executable( ert_util_parent_path ert_util_parent_path.c )
target_link_libraries( ert_util_parent_path ert_util test_util )
add_test( ert_util_parent_path ${EXECUTABLE_OUTPUT_PATH}/ert_util_parent_path)

add_executable( ert_util_alloc_file_components ert_util_alloc_file_components.c )
target_link_libraries( ert_util_alloc_file_components ert_util test_util )
add_test( ert_util_alloc_file_components ${EXECUTABLE_OUTPUT_PATH}/ert_util_alloc_file_components)

add_executable( ert_util_work_area ert_util_work_area.c )
target_link_libraries( ert_util_work_area ert_util test_util )
add_test( NAME ert_util_work_area 
          COMMAND ${EXECUTABLE_OUTPUT_PATH}/ert_util_work_area data2/file1 ${CMAKE_CURRENT_SOURCE_DIR}/data2/file2 data2
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})



if (WITH_LATEX)
   add_executable( ert_util_latex_test ert_util_latex_test.c )
   target_link_libraries(ert_util_latex_test ert_util test_util )
   add_test( ert_util_latex_test ${EXECUTABLE_OUTPUT_PATH}/ert_util_latex_test ${CMAKE_CURRENT_SOURCE_DIR}/data/latex_OK.tex  latex_OK.tex)
endif()


add_executable( ert_util_arg_pack ert_util_arg_pack.c)
set_target_properties( ert_util_arg_pack PROPERTIES COMPILE_FLAGS "-Werror")
target_link_libraries( ert_util_arg_pack ert_util test_util )
add_test( ert_util_arg_pack ${EXECUTABLE_OUTPUT_PATH}/ert_util_arg_pack)

add_executable( ert_util_abort_gnu_tests ert_util_abort_gnu_tests.c)
target_link_libraries( ert_util_abort_gnu_tests ert_util test_util)
add_test( ert_util_abort_gnu_tests ${EXECUTABLE_OUTPUT_PATH}/ert_util_abort_gnu_tests)

if (ERT_BUILD_BLOCK_FS)
   add_executable( ert_util_block_fs ert_util_block_fs.c)
   target_link_libraries( ert_util_block_fs ert_util test_util)
   add_test( ert_util_block_fs ${EXECUTABLE_OUTPUT_PATH}/ert_util_block_fs)
endif()   
