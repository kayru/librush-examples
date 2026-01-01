#version 450

layout (set = 0, binding = 0) uniform Constants
{
	vec4 params;
} globals;

layout (location = 0) out vec4 fragColor;

float noise(vec2 t, float s)
{
	return fract(sin(s + dot(t.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float bouncy(vec2 v, vec2 fragCoord, vec4 params)
{
	vec2 cp = v * params.z;
	vec2 cp_wrap = floor(cp / params.xy);
	cp = mod(cp, params.xy);
	cp = mix(cp, params.xy - cp, mod(cp_wrap, vec2(2.0)));
	return 25.0 / (1.0 + length(cp - fragCoord.xy));
}

void main()
{
	vec3 res = vec3(0.0);
	vec2 fragCoord = gl_FragCoord.xy;
	res += vec3(1.0, 0.3, 0.2) * bouncy(vec2(211.0, 312.0), fragCoord, globals.params);
	res += vec3(0.3, 1.0, 0.2) * bouncy(vec2(312.0, 210.0), fragCoord, globals.params);
	res += vec3(0.2, 0.3, 1.0) * bouncy(vec2(331.0, 130.0), fragCoord, globals.params);
	float dither = (noise(fragCoord, globals.params.z) - 0.5) / 64.0;
	fragColor = vec4(res + dither, 1.0);
}
