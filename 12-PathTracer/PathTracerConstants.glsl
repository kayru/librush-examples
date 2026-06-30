#ifndef INCLUDED_PATH_TRACER_CONSTANTS
#define INCLUDED_PATH_TRACER_CONSTANTS

// Constants shared by the C++ host, the GLSL (Vulkan) shaders and the Metal shader.
// Only preprocessor defines belong here so the file is valid in all three.

#define PT_FLAG_USE_ENVMAP                 (1u << 0u)
#define PT_FLAG_USE_NEUTRAL_BACKGROUND     (1u << 1u)
#define PT_FLAG_USE_DEPTH_OF_FIELD         (1u << 2u)
#define PT_FLAG_USE_NORMAL_MAPPING         (1u << 3u)
#define PT_FLAG_DEBUG_SIMPLE_SHADING       (1u << 4u)
#define PT_FLAG_DEBUG_DISABLE_ACCUMULATION (1u << 5u)
#define PT_FLAG_DEBUG_HIT_MASK             (1u << 6u)
#define PT_FLAG_DEBUG_FOCAL_PLANE          (1u << 7u)

#define PT_DEBUG_VIS_NONE              0u
#define PT_DEBUG_VIS_ALBEDO           1u
#define PT_DEBUG_VIS_GEO_NORMAL       2u
#define PT_DEBUG_VIS_SHADING_NORMAL   3u
#define PT_DEBUG_VIS_NORMAL_MAPPED    4u
#define PT_DEBUG_VIS_TANGENT          5u
#define PT_DEBUG_VIS_BITANGENT        6u
#define PT_DEBUG_VIS_METALNESS        7u
#define PT_DEBUG_VIS_ROUGHNESS        8u
#define PT_DEBUG_VIS_UV               9u

#define PT_MATERIAL_MODE_PBR_METALLIC_ROUGHNESS  0u
#define PT_MATERIAL_MODE_PBR_SPECULAR_GLOSSINESS 1u

#define PT_MAX_TEXTURES 1024

#endif // INCLUDED_PATH_TRACER_CONSTANTS
