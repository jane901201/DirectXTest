set(EXAMPLE_SOURCES
    rt_01_ppm.cpp
)

foreach (example_src IN LISTS EXAMPLE_SOURCES)
    get_filename_component(example_name ${example_src} NAME_WE)

    add_executable(${example_name} ${example_src})

    target_compile_features(${example_name} PRIVATE cxx_std_17)

    if(MSVC)
        target_compile_options(${example_name} PRIVATE /EHsc)
    endif ()
endforeach ()