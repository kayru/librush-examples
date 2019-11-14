#ifndef INCLUDED_BRDF_GLSL
#define INCLUDED_BRDF_GLSL

#include "Common.glsl"

// Adapted from Filament: https://github.com/google/filament

float D_GGX(float linearRoughness, float NoH, vec3 h)
{
	float oneMinusNoHSquared = 1.0 - NoH * NoH;
	float a = NoH * linearRoughness;
	float k = linearRoughness / (oneMinusNoHSquared + a * a);
	float d = k * k * (1.0 / M_PI);
	return saturate(d);
}

vec3 importanceSamplingNdfDggx(vec2 uv, float linearRoughness) {
	// Importance sampling D_GGX
	float a2 = linearRoughness * linearRoughness;
	float phi = 2.0 * M_PI * uv.x;
	float cosTheta2 = (1.0 - uv.y) / (1.0 + (a2 - 1.0) * uv.y);
	float cosTheta = sqrt(cosTheta2);
	float sinTheta = sqrt(1.0 - cosTheta2);
	return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float V_SmithGGXCorrelated(float linearRoughness, float NoV, float NoL)
{
	float v = 0.5 / mix(2.0 * NoL * NoV, NoL + NoV, linearRoughness);
	return saturate(v);
}

vec3 F_Schlick(vec3 f0, float f90, float VoH)
{
    float f = pow5(1.0 - VoH);
    return f + f0 * (f90 - f);
}

float F_Schlick(float f0, float f90, float VoH)
{
    return f0 + (f90 - f0) * pow5(1.0 - VoH);
}

float BRDF_Distribution(float linearRoughness, float NoH, vec3 h)
{
	return D_GGX(linearRoughness, NoH, h);
}

float BRDF_Visibility(float linearRoughness, float NoV, float NoL, float LoH)
{
	return V_SmithGGXCorrelated(linearRoughness, NoV, NoL);
}

vec3 BRDF_Fresnel(vec3 f0, float LoH)
{
	float f90 = saturate(dot(f0, vec3(50.0 * 0.33)));
	return F_Schlick(f0, f90, LoH);
}

float BRDF_Diffuse()
{
	return 1.0 / M_PI;
}

vec3 evalSpecular(vec3 f0, float linearRoughness, float NoH, float NoV, float NoL, float LoH, vec3 H)
{
	float D = BRDF_Distribution(linearRoughness, NoH, H);
	float V = BRDF_Visibility(linearRoughness, NoV, NoL, LoH);
	vec3  F = BRDF_Fresnel(f0, LoH);
	return (D * V) * F;
}

vec3 evalDiffuse(vec3 albedo)
{
	return albedo * BRDF_Diffuse();
}

#endif // INCLUDED_BRDF_GLSL
