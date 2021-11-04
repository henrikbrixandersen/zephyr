# SPDX-License-Identifier: Apache-2.0

# These functions can be used to generate a CANopen object dictionary
# implementation from a CANopen Electronic Data Sheet (EDS) file.
function(generate_canopen_objdict
    input_file         # The Electronic Data Sheet (EDS) file
    output_header_file # The generated header file
    output_impl_file   # The generated implementation file
    )
  add_custom_command(
    OUTPUT ${output_impl_file} ${output_header_file}
    COMMAND
    ${PYTHON_EXECUTABLE}
    ${ZEPHYR_BASE}/scripts/gen_canopen_objdict.py
    --zephyr-base ${ZEPHYR_BASE}
    --bindir ${CMAKE_BINARY_DIR}
    --input ${input_file}
    --header ${output_header_file}
    --impl ${output_impl_file}
    ${ARGN} # Extra arguments are passed to gen_canopen_objdict.py
    DEPENDS ${input_file} ${ZEPHYR_BASE}/scripts/gen_canopen_objdict.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()

function(generate_canopen_objdict_for_gen_target
    target             # The cmake target that depends on the generated file
    input_file         # The Electronic Data Sheet (EDS) file
    output_header_file # The generated header file
    output_impl_file   # The generated implementation file
    gen_target         # The generated file target we depend on
                       # Any additional arguments are passed on to gen_canopen_objdict.py
    )
  generate_canopen_objdict(${input_file} ${output_header_file} ${output_impl_file} ${ARGN})

  # Ensure 'output_impl_file' and 'output_header_file' are generated before
  # 'target' by creating a dependency between the two targets

  add_dependencies(${target} ${gen_target})
endfunction()

function(generate_canopen_objdict_for_target
    target             # The cmake target that depends on the generated file
    input_file         # The Electronic Data Sheet (EDS) file
    output_header_file # The generated header file
    output_impl_file   # The generated implementation file
                       # Any additional arguments are passed on to gen_canopen_objdict.py
    )
  # Ensure 'output_impl_file' and 'output_header_file' are generated before
  # 'target' by creating a 'custom_target' for them and setting up a dependency
  # between the two targets

  # But first create a unique name for the custom target
  generate_unique_target_name_from_filename(${output_impl_file} generated_target_name)

  add_custom_target(${generated_target_name} DEPENDS ${output_header_file} ${output_impl_file})
  generate_canopen_objdict_for_gen_target(${target} ${input_file}
    ${output_header_file} ${output_impl_file} ${generated_target_name} ${ARGN})
endfunction()
