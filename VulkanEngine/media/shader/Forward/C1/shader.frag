#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "../common_defines.glsl"
#include "../light.glsl"

layout(set = 0, binding = 0) uniform cameraUBO_t {
    cameraData_t data;
} cameraUBO;

layout(set = 1, binding = 0) uniform lightUBO_t {
    lightData_t data;
} lightUBO;

layout(location = 0) in vec3 fragNormal; //
layout(location = 1) in vec2 fragTexCoord;//
layout(location = 2) in vec4 fragPosition;//
layout(location = 3) in vec4 fragColor;//

layout(location = 0) out vec4 outColor;

// layout(set = 2, binding = 0) uniform sampler2D shadowMap;
// layout(set = 2, binding = 0) uniform sampler2D texSampler;


void main() {
    vec4 ambientC = lightUBO.data.col_ambient; //X
    vec4 diffuseC = lightUBO.data.col_diffuse;
    vec4 specularC = lightUBO.data.col_specular;
    vec4 lightPositon = lightUBO.data.lightModel[3]; //
    float s = 2.5f;
    vec4 n =normalize(vec4(fragNormal.x,fragNormal.y,fragNormal.z, 0.0f));//
    n = vec4(0.0,1.0,0,1.0);
    // vec4 fragColor = texture(texSampler, fragTexCoord);
    vec4 cameraPosition = inverse(cameraUBO.data.camView)[3]; //
    vec4 V = normalize(lightPositon - fragPosition);//
    vec4 L = normalize(cameraPosition - fragPosition);//
    vec4 H = normalize(V+L);//

    vec4 diffuse = max(dot(n, L)*4, 0.0f) * diffuseC;
    vec4 specular = pow(max(dot(n, H)*4, 0.0f), s) * specularC;

    outColor = (ambientC +diffuse+ specular) * fragColor;
}
