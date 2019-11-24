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

// http://jcgt.org/published/0007/04/01/paper.pdf
vec3 importanceSampleDGGXVNDF(vec2 uv, float linearRoughness, vec3 V)
{
	// Section 3.2: transforming the view direction to the hemisphere configuration
	vec3 Vh = normalize(vec3(linearRoughness * V.x, linearRoughness * V.y, V.z));

	// Section 4.1: orthonormal basis (with special case if cross product is zero)
	float lensq = pow2(Vh.x) + pow2(Vh.y);
	vec3 T1 = lensq > 0.0 ? vec3(-Vh.y, Vh.x, 0.0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
	vec3 T2 = cross(Vh, T1);

	// Section 4.2: parameterization of the projected area
	float r = sqrt(uv.x);
	float phi = 2.0 * M_PI * uv.y;
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5 * (1.0 + Vh.z);
	t2 = (1.0 - s) * sqrt(1.0 - pow2(t1)) + s * t2;

	// Section 4.3: reprojection onto hemisphere
	vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - pow2(t1) - pow2(t2))) * Vh;

	// Section 3.4: transforming the normal back to the ellipsoid configuration
	return vec3(linearRoughness * Nh.x, linearRoughness * Nh.y, max(0.0, Nh.z));
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
