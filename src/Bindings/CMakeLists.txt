# Python module build
pybind11_add_module(cg_engine_bindings MODULE # <--- Corrected Name
    CoreBindings.cpp
    # Add other binding source files here
)

target_compile_features(cg_engine_bindings PRIVATE cxx_std_17) # <-- Added consistency

target_link_libraries(cg_engine_bindings PRIVATE # <--- Corrected Name
    pybind11::module
    CGEngineCore # Make sure this matches your engine library target name
)

# Optional but recommended: Explicitly include project includes if not inherited
target_include_directories(cg_engine_bindings PRIVATE ${PROJECT_INCLUDES})

set_target_properties(cg_engine_bindings PROPERTIES # <--- Corrected Name
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" # <-- Changed to match executable output
)