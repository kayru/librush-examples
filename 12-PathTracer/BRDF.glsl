#ifndef INCLUDED_BRDF_GLSL
#define INCLUDED_BRDF_GLSL

#include "Common.glsl"

// Adapted from Filament: https://github.com/google/filament

float D_GGX(float linearRoughness, float NoH)
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

vec3 evalSpecular(vec3 f0, float linearRoughness, float NoH, float NoV, float NoL, float LoH, vec3 H)
{
	float D = D_GGX(linearRoughness, NoH);
	float V = V_SmithGGXCorrelated(linearRoughness, NoV, NoL);
	vec3  F = F_Schlick(f0, 1.0, LoH);
	return (D * V) * F;
}

vec3 evalDiffuse(vec3 diffuseColor)
{
	return diffuseColor / M_PI;
}

vec3 evalSurface(Surface s, vec3 V, vec3 L)
{
	vec3 H = normalize(V + L);
	vec3 N = s.normal;

	float NoV = saturate(dot(N, V));
	float NoL = saturate(dot(N, L));
	float NoH = saturate(dot(N, H));
	float LoH = saturate(dot(L, H));

	vec3 Fr = evalSpecular(s.specularColor, s.linearRoughness, NoH, NoV, NoL, LoH, H);
	vec3 Fd = evalDiffuse(s.diffuseColor);

	return (Fr + Fd) * NoL;
}


#endif // INCLUDED_BRDF_GLSL
