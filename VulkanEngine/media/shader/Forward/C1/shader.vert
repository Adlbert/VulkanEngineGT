#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"

layout(set = 0, binding = 0) uniform cameraUBO_t {
    cameraData_t data;
} cameraUBO;

layout(set = 3, binding = 0) uniform objectUBO_t {
    objectData_t data;
} objectUBO;

layout(location = 0) in vec3 inPositionL;
layout(location = 1) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec4 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragPosition;
layout(location = 3) out vec4 fragColor;

out gl_PerVertex {
    vec4 gl_Position;
};


void main() {
    gl_Position = cameraUBO.data.camProj  * cameraUBO.data.camView * objectUBO.data.model * vec4(inPositionL, 1.0);
    fragPosition = objectUBO.data.model * vec4(inPositionL, 1.0);
    fragColor   = objectUBO.data.color;
    // fragNormal = normalize(objectUBO.data.model * vec4( inNormal,   1.0 ));
    fragNormal = normalize(vec4( inNormal, 1.0 ));
    fragTexCoord = inTexCoord;
}
