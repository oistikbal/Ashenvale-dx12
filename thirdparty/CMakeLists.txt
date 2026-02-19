option(FLECS_STATIC "Build static flecs lib" ON)
add_subdirectory(flecs)
add_subdirectory(D3D12MA)
add_subdirectory(fastgltf)

if(TARGET flecs)
    set_target_properties(flecs PROPERTIES FOLDER "ThirdParty")
endif()

if(TARGET flecs_static)
    set_target_properties(flecs_static PROPERTIES FOLDER "ThirdParty")
endif()

if(TARGET D3D12MemoryAllocator)
    set_target_properties(D3D12MemoryAllocator PROPERTIES FOLDER "ThirdParty")
endif()

if(TARGET fastgltf)
    set_target_properties(fastgltf PROPERTIES FOLDER "ThirdParty")
endif()
