#ifndef INCLUDED_PATH_TRACER_CORE
#define INCLUDED_PATH_TRACER_CORE

// Include after the backend has declared MaterialConstants, Vertex and the
// PathTracerContext/PtHit/PtPayload types.

// Caller resolves material + index base (firstIndex + primId*3 for SBT geometry,
// primId*3 for inline backends).
SHADER_INLINE void fillPayload(PathTracerContext ctx, PtHit hit, uint indexBase,
	MaterialConstants material, INOUT(PtPayload) pl)
{
	pl.hitT = hit.t;

	vec3 bary = vec3(1.0f - hit.bary.x - hit.bary.y, hit.bary.x, hit.bary.y);

	Vertex v0 = PT_VERTEX(ctx, PT_INDEX(ctx, indexBase + 0u));
	Vertex v1 = PT_VERTEX(ctx, PT_INDEX(ctx, indexBase + 1u));
	Vertex v2 = PT_VERTEX(ctx, PT_INDEX(ctx, indexBase + 2u));

	vec3 p0 = PT_VTX_POS(v0);
	vec3 p1 = PT_VTX_POS(v1);
	vec3 p2 = PT_VTX_POS(v2);

	vec3 normal = normalize(PT_VTX_NRM(v0) * bary.x + PT_VTX_NRM(v1) * bary.y + PT_VTX_NRM(v2) * bary.z);
	pl.geoNormal = normalize(cross(p1 - p0, p2 - p0));

	vec2 uv = PT_VTX_UV(v0) * bary.x + PT_VTX_UV(v1) * bary.y + PT_VTX_UV(v2) * bary.z;
	pl.texcoord = uv;

	vec4 albedoSample = vec4(1.0f);
	vec4 specularSample = vec4(1.0f);
	if (material.albedoTextureId < PT_MAX_TEXTURES)
	{
		albedoSample = PT_TEXTURE(ctx, material.albedoTextureId, uv);
	}
	if (material.specularTextureId < PT_MAX_TEXTURES)
	{
		specularSample = PT_TEXTURE(ctx, material.specularTextureId, uv);
	}

	if (material.materialMode == PT_MATERIAL_MODE_PBR_METALLIC_ROUGHNESS)
	{
		pl.baseColor = albedoSample.xyz * material.albedoFactor.xyz;
		pl.metalness = material.metallicFactor * specularSample.z;
		pl.roughness = material.roughnessFactor * specularSample.y;
	}
	else
	{
		pl.metalness = max3(specularSample.xyz * material.specularFactor.xyz);
		pl.roughness = 1.0f - specularSample.w * material.roughnessFactor;
		pl.baseColor = mix(albedoSample.xyz * material.albedoFactor.xyz,
			specularSample.xyz * material.specularFactor.xyz, pl.metalness);
	}

	pl.reflectance = material.reflectance;
	pl.shadingNormal = normal;
	pl.normal = normal;
	pl.tangent = vec3(0.0f);
	pl.bitangent = vec3(0.0f);

	vec4 tangent = PT_VTX_TAN(v0) * bary.x + PT_VTX_TAN(v1) * bary.y + PT_VTX_TAN(v2) * bary.z;
	float tangentLen = length(tangent.xyz);
	bool hasTangent = false;
	bool hasBitangent = false;
	if (tangentLen > 1e-5f)
	{
		vec3 tanU = tangent.xyz / tangentLen;
		vec3 tanV = cross(pl.shadingNormal, tanU) * tangent.w;
		float bitangentLen = length(tanV);
		if (bitangentLen > 1e-5f)
		{
			tanV = tanV / bitangentLen;
			hasBitangent = true;
		}
		pl.tangent = tanU;
		pl.bitangent = tanV;
		hasTangent = true;
	}

	bool useNormalMapping = (PT_SCENE(ctx, flags) & PT_FLAG_USE_NORMAL_MAPPING) != 0u
		&& material.normalTextureId != 0u
		&& material.normalTextureId < PT_MAX_TEXTURES;
	if (useNormalMapping && hasTangent && hasBitangent)
	{
		vec3 normalSample = PT_TEXTURE(ctx, material.normalTextureId, uv).xyz * 2.0f - 1.0f;
		normalSample.z = sqrt(max(0.0f, 1.0f - normalSample.x * normalSample.x - normalSample.y * normalSample.y));
		mat3 basis = mat3(pl.tangent, pl.bitangent, pl.normal);
		pl.normal = normalize(basis * normalSample);
	}

	if (!hit.frontFacing)
	{
		pl.shadingNormal = -pl.shadingNormal;
		pl.normal = -pl.normal;
		pl.geoNormal = -pl.geoNormal;
		pl.tangent = -pl.tangent;
		pl.bitangent = -pl.bitangent;
	}
}

// Everything below is the render loop, only needed by configs that trace from the kernel.
#ifdef PT_HAS_RENDER_LOOP

SHADER_INLINE vec3 encodeSignedVector(vec3 v)
{
	float len = length(v);
	if (len < 1e-5f)
	{
		return vec3(0.5f);
	}
	return v / len * 0.5f + 0.5f;
}

SHADER_INLINE vec3 debugVisColor(uint mode, PtPayload payload, vec2 uv)
{
	switch (mode)
	{
	case PT_DEBUG_VIS_ALBEDO:         return payload.baseColor;
	case PT_DEBUG_VIS_GEO_NORMAL:     return encodeSignedVector(payload.geoNormal);
	case PT_DEBUG_VIS_SHADING_NORMAL: return encodeSignedVector(payload.shadingNormal);
	case PT_DEBUG_VIS_NORMAL_MAPPED:  return encodeSignedVector(payload.normal);
	case PT_DEBUG_VIS_TANGENT:        return encodeSignedVector(payload.tangent);
	case PT_DEBUG_VIS_BITANGENT:      return encodeSignedVector(payload.bitangent);
	case PT_DEBUG_VIS_METALNESS:      return vec3(payload.metalness);
	case PT_DEBUG_VIS_ROUGHNESS:      return vec3(payload.roughness);
	case PT_DEBUG_VIS_UV:             return vec3(fract(uv.x), fract(uv.y), 0.0f);
	default:                          return vec3(0.0f);
	}
}

// Envmap-space direction -> world direction (the inverse maps world -> envmap-space below).
SHADER_INLINE vec3 envmapToWorld(PathTracerContext ctx, vec3 dir)
{
	return dir * toMat3(PT_SCENE(ctx, matEnvmapTransform));
}

SHADER_INLINE vec3 worldToEnvmap(PathTracerContext ctx, vec3 dir)
{
	return toMat3(PT_SCENE(ctx, matEnvmapTransform)) * dir;
}

SHADER_INLINE LightSample sampleEnvmap(PathTracerContext ctx, vec3 envmapDir)
{
	vec4 s = PT_ENVMAP(ctx, cartesianToLatLongTexcoord(envmapDir));
	LightSample r;
	r.w = envmapToWorld(ctx, envmapDir);
	r.value = s.xyz;
	r.pdfW = s.w;
	return r;
}

SHADER_INLINE vec3 envMapPixelIndexToDirection(PathTracerContext ctx, uint idx, vec2 jitter)
{
	ivec2 envmapSize = PT_SCENE(ctx, envmapSize);
	uint x = idx % uint(envmapSize.x);
	uint y = idx / uint(envmapSize.x);
	vec2 uv = (vec2(float(x), float(y)) + jitter) / vec2(envmapSize);
	return latLongTexcoordToCartesian(uv);
}

SHADER_INLINE vec3 importanceSampleSkyLightDir(PathTracerContext ctx, INOUT(uint) randomSeed)
{
	ivec2 envmapSize = PT_SCENE(ctx, envmapSize);
	if (envmapSize.x <= 0 || envmapSize.y <= 0 || !PT_ENVDIST_VALID(ctx))
	{
		return vec3(0.0f, 1.0f, 0.0f);
	}

	uint texelCount = uint(envmapSize.x * envmapSize.y);
	uint i = randomUint32(randomSeed) % texelCount;
	EnvmapCell entry = PT_ENVDIST(ctx, i);
	vec2 jitter = randomFloat2(randomSeed);

	if (randomFloat(randomSeed) <= entry.p)
	{
		return envMapPixelIndexToDirection(ctx, i, jitter);
	}
	return envMapPixelIndexToDirection(ctx, entry.i, jitter);
}

SHADER_INLINE LightSample importanceSampleEnvmap(PathTracerContext ctx, INOUT(uint) randomSeed)
{
	return sampleEnvmap(ctx, importanceSampleSkyLightDir(ctx, randomSeed));
}

// Trace wrappers: closest hit (fills payload) and any hit (shadow). One pair per config.
#ifdef __METAL_VERSION__

SHADER_INLINE PtHit toPtHit(intersection_result<triangle_data, instancing> res)
{
	PtHit hit;
	hit.valid = res.type != intersection_type::none;
	hit.t = res.distance;
	hit.primId = res.primitive_id;
	hit.bary = res.triangle_barycentric_coord;
	hit.frontFacing = res.triangle_front_facing;
	return hit;
}

SHADER_INLINE MaterialConstants resolveMaterial(PathTracerContext ctx, uint primId)
{
	MaterialConstants material;
	material.albedoFactor = float4(1.0f);
	material.specularFactor = float4(1.0f);
	material.albedoTextureId = 0u;
	material.specularTextureId = 0u;
	material.normalTextureId = 0u;
	material.firstIndex = 0u;
	material.alphaMode = 0u;
	material.metallicFactor = 1.0f;
	material.roughnessFactor = 1.0f;
	material.reflectance = 0.04f;
	material.materialMode = PT_MATERIAL_MODE_PBR_METALLIC_ROUGHNESS;

	uint materialIndex = 0u;
	if (ctx.s0->materialIndices)
	{
		materialIndex = ctx.s0->materialIndices[primId];
	}
	if (ctx.s0->materials)
	{
		material = ctx.s0->materials[materialIndex];
	}
	return material;
}

SHADER_INLINE bool ptTraceFill(PathTracerContext ctx, PtRay r, INOUT(PtPayload) pl)
{
	intersector<triangle_data, instancing> it;
	it.assume_geometry_type(geometry_type::triangle);
	it.set_triangle_cull_mode(triangle_cull_mode::none);

	ray mr;
	mr.origin = r.origin;
	mr.direction = r.direction;
	mr.min_distance = r.minT;
	mr.max_distance = r.maxT;

	PtHit hit = toPtHit(it.intersect(mr, ctx.s0->tlas));
	if (!hit.valid)
	{
		return false;
	}
	fillPayload(ctx, hit, hit.primId * 3u, resolveMaterial(ctx, hit.primId), pl);
	return true;
}

SHADER_INLINE bool ptTraceShadow(PathTracerContext ctx, PtRay r)
{
	intersector<triangle_data, instancing> it;
	it.assume_geometry_type(geometry_type::triangle);
	it.set_triangle_cull_mode(triangle_cull_mode::none);

	ray mr;
	mr.origin = r.origin;
	mr.direction = r.direction;
	mr.min_distance = r.minT;
	mr.max_distance = r.maxT;

	return it.intersect(mr, ctx.s0->tlas).type != intersection_type::none;
}

#elif defined(PT_CONFIG_SBT_RAYGEN)

bool ptTraceFill(PathTracerContext ctx, PtRay r, INOUT(PtPayload) pl)
{
	sbtPayload.hitT = 0.0;
	traceRayEXT(TLAS, gl_RayFlagsOpaqueEXT, 0xFFu, 0u, 1u, 0u,
		r.origin, r.minT, r.direction, r.maxT, 0);
	pl = sbtPayload;
	return sbtPayload.hitT >= 0.0;
}

bool ptTraceShadow(PathTracerContext ctx, PtRay r)
{
	float savedHitT = sbtPayload.hitT;
	sbtPayload.hitT = 0.0;
	traceRayEXT(TLAS,
		gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
		0xFFu, 0u, 1u, 0u,
		r.origin, r.minT, r.direction, r.maxT, 0);
	bool isHit = sbtPayload.hitT >= 0.0;
	sbtPayload.hitT = savedHitT;
	return isHit;
}

#endif

// Entry point sets up the context; this reads/writes the output image and focus buffer.
SHADER_INLINE void ptRenderPixel(PathTracerContext ctx, ivec2 pixelIndex)
{
	const bool useDebugFurnace = false;
	const bool useDebugReflections = false;
	const bool useDirectLighting = true;
	const bool useIndirectSpecular = true;
	const bool useRoughnessBias = true;
	const bool visNormal = false;

	ivec2 outputSize = PT_SCENE(ctx, outputSize);
	vec2 pixelUV = vec2(pixelIndex) / vec2(outputSize);

	uint pixelLinearIndex = uint(pixelIndex.x + pixelIndex.y * outputSize.x);
	uint pixelRandomSeed = hashFnv1(pixelLinearIndex + pixelLinearIndex * 1294974679u);
	uint randomSeed = hashFnv1(pixelRandomSeed + PT_SCENE(ctx, frameIndex));
	vec2 pixelJitter = (randomFloat2(randomSeed) - 0.5f) / vec2(outputSize);

	vec3 result = vec3(0.0f);
	vec3 throughput = vec3(1.0f);

	PtRay primaryRay;
	primaryRay.minT = 0.0f;
	primaryRay.maxT = 1e9f;
	primaryRay.origin = PT_SCENE(ctx, cameraPosition).xyz;
	primaryRay.direction = getCameraViewVector(pixelUV + pixelJitter, PT_SCENE(ctx, matView), PT_SCENE(ctx, matProj));

	if ((PT_SCENE(ctx, flags) & PT_FLAG_USE_DEPTH_OF_FIELD) != 0u)
	{
		float denom = dot(PT_SCENE(ctx, matView)[2].xyz, primaryRay.direction);
		vec3 focusPoint = primaryRay.origin + primaryRay.direction * (PT_SCENE(ctx, focusDistance) / denom);
		vec2 apertureSample = sampleUniformDisk(randomSeed) * 0.5f;
		primaryRay.origin += (PT_SCENE(ctx, matView)[0].xyz * apertureSample.x
			+ PT_SCENE(ctx, matView)[1].xyz * apertureSample.y) * PT_SCENE(ctx, apertureSize);
		primaryRay.direction = normalize(focusPoint - primaryRay.origin);
	}

	uint maxPathLength = useDebugFurnace ? 2u : 5u;
	float scatterPdfW = 1e9f;
	float roughnessBias = 0.0f;
	bool useEnvmap = (PT_SCENE(ctx, flags) & PT_FLAG_USE_ENVMAP) != 0u;
	bool debugSimple = (PT_SCENE(ctx, flags) & PT_FLAG_DEBUG_SIMPLE_SHADING) != 0u;
	bool debugHitMask = (PT_SCENE(ctx, flags) & PT_FLAG_DEBUG_HIT_MASK) != 0u;
	uint debugVisMode = PT_SCENE(ctx, debugVisMode);
	bool debugVisEnabled = debugVisMode != PT_DEBUG_VIS_NONE;
	bool showFocalPlane = (PT_SCENE(ctx, flags) & PT_FLAG_DEBUG_FOCAL_PLANE) != 0u;
	bool skipAccum = (PT_SCENE(ctx, flags) & PT_FLAG_DEBUG_DISABLE_ACCUMULATION) != 0u;
	float focalOverlay = 0.0f;
	float primaryDepth = -1.0f; // primary-hit depth for the focus feedback buffer

	// Single-bounce debug visualisations (hit mask / simple shading / G-buffer channels).
	if (debugSimple || debugHitMask || debugVisEnabled)
	{
		PtPayload payload;
		bool isHit = ptTraceFill(ctx, primaryRay, payload);
		if (debugHitMask)
		{
			vec3 maskColor = isHit ? vec3(1.0f, 0.0f, 0.0f) : vec3(0.0f);
			PT_OUTPUT_WRITE(ctx, pixelIndex, maskColor);
			return;
		}

		vec3 simpleColor = vec3(0.0f);
		if (!isHit)
		{
			if (debugSimple)
			{
				simpleColor = useEnvmap
					? sampleEnvmap(ctx, worldToEnvmap(ctx, primaryRay.direction)).value
					: getSkyColor(primaryRay.direction);
				if ((PT_SCENE(ctx, flags) & PT_FLAG_USE_NEUTRAL_BACKGROUND) != 0u)
				{
					simpleColor = vec3(0.25f);
				}
			}
		}
		else
		{
			simpleColor = debugVisEnabled ? debugVisColor(debugVisMode, payload, payload.texcoord) : payload.baseColor;
		}

		if (!skipAccum)
		{
			float frame = float(PT_SCENE(ctx, frameIndex));
			simpleColor = (PT_OUTPUT_READ(ctx, pixelIndex) * frame + simpleColor) / (frame + 1.0f);
		}
		PT_OUTPUT_WRITE(ctx, pixelIndex, simpleColor);
		return;
	}

	for (uint i = 0u; i <= maxPathLength; ++i)
	{
		PtPayload payload;
		bool isHit = ptTraceFill(ctx, primaryRay, payload);

		if (useDebugFurnace && i > 0u)
		{
			isHit = false;
		}

		if (isHit)
		{
			if (useRoughnessBias)
			{
				// Path roughening ("Avoiding Caustic Paths", Arnold), keeps fireflies down.
				payload.roughness = max(payload.roughness, roughnessBias);
				roughnessBias = payload.roughness;
			}
			if (i == 0u)
			{
				float hitDepth = payload.hitT * dot(PT_SCENE(ctx, matView)[2].xyz, primaryRay.direction);
				primaryDepth = hitDepth;
				if (showFocalPlane)
				{
					focalOverlay = focalPlaneOverlay(hitDepth, PT_SCENE(ctx, focusDistance), PT_SCENE(ctx, apertureSize),
						PT_SCENE(ctx, focalLength), PT_SCENE(ctx, cameraSensorSize).x,
						float(PT_SCENE(ctx, outputSize).x), PT_SCENE(ctx, focalPlaneFalloffPx));
				}
			}
		}

		if (!isHit)
		{
			if (i == 0u && (PT_SCENE(ctx, flags) & PT_FLAG_USE_NEUTRAL_BACKGROUND) != 0u)
			{
				result = vec3(0.25f);
			}
			else if (useEnvmap)
			{
				LightSample ls = sampleEnvmap(ctx, worldToEnvmap(ctx, primaryRay.direction));
				float misWeight = powerHeuristic(scatterPdfW, ls.pdfW);
				result += throughput * ls.value * misWeight;
			}
			else
			{
				result += throughput * getSkyColor(primaryRay.direction);
			}
			break;
		}

		if (visNormal)
		{
			result = payload.normal * 0.5f + 0.5f;
			break;
		}

		if (useDebugFurnace)
		{
			payload.baseColor = vec3(1.0f);
		}
		if (useDebugReflections && i == 0u)
		{
			payload.baseColor = vec3(1.0f);
		}

		vec3 N = normalize(payload.normal);
		vec3 V = -primaryRay.direction;

		Surface surf = unpackSurface(payload.baseColor, payload.metalness, payload.roughness, payload.reflectance);
		vec3 diffuseColor = surf.diffuseColor;
		vec3 specularColor = surf.specularColor;
		float linearRoughness = surf.linearRoughness;
		if (i == maxPathLength)
		{
			break;
		}

		if (useDirectLighting && !useDebugFurnace && (!useDebugReflections || i > 0u))
		{
			vec3 L = vec3(0.0f, 0.0f, 1.0f);
			vec3 lightColor = vec3(0.0f);
			float lightPdfW = 0.0f;

			if (useEnvmap)
			{
				LightSample ls = importanceSampleEnvmap(ctx, randomSeed);
				L = ls.w;
				lightColor = ls.value;
				lightPdfW = ls.pdfW;
			}
			else
			{
				L = getSunDirection();
				lightColor = getSunColor();
				lightPdfW = 1.0f;
			}

			PtRay shadowRay;
			shadowRay.origin = primaryRay.origin + primaryRay.direction * payload.hitT;
			shadowRay.origin += payload.geoNormal * max3(abs(shadowRay.origin)) * 1e-4f;
			shadowRay.direction = L;
			shadowRay.minT = 0.0f;
			shadowRay.maxT = 1e9f;

			float NoL = dot(N, L);
			if (NoL > 0.0f && lightPdfW > 0.0f && !ptTraceShadow(ctx, shadowRay))
			{
				vec3 H = normalize(V + L);
				float NoH = max(0.0f, dot(N, H));
				float LoH = max(0.0f, dot(L, H));
				float VoH = max(0.0f, dot(V, H));

				float sD = D_GGX(linearRoughness, NoH);
				float sG = G1_Smith(linearRoughness, NoL);
				vec3 sF = F_Schlick(specularColor, 1.0f, LoH);

				float denom = 4.0f * VoH;
				float brdfPdfW = denom > 0.0f ? sD * NoH / denom : 0.0f;
				vec3 brdf = sD * sG * sF;

				float misWeight = useEnvmap ? powerHeuristic(lightPdfW, brdfPdfW) : 1.0f;
				result += throughput * lightColor * brdf * misWeight / (lightPdfW * 2.0f);

				float diffusePdfW = 1.0f / M_PI;
				vec3 diffuseBrdf = diffuseColor * NoL;
				misWeight = useEnvmap ? powerHeuristic(lightPdfW, diffusePdfW) : 1.0f;
				result += throughput * lightColor * diffuseBrdf * misWeight / (lightPdfW * 2.0f);
			}
		}

		// Generate next ray.
		primaryRay.origin = primaryRay.origin + primaryRay.direction * payload.hitT;
		primaryRay.origin += payload.geoNormal * max3(abs(primaryRay.origin)) * 1e-4f;

		vec2 reflectionSampleUV = randomFloat2(randomSeed);
		if (i == 0u)
		{
			uint pixelRandomSeedState = pixelRandomSeed;
			vec2 base = Halton23(int(PT_SCENE(ctx, frameIndex))) + randomFloat2(pixelRandomSeedState);
			reflectionSampleUV = base - floor(base);
		}

		float specularProbability = useIndirectSpecular ? clamp(payload.metalness, 0.1f, 0.9f) : 0.0f;
		bool isSpecular = randomFloat(randomSeed) <= specularProbability;

		if (isSpecular)
		{
			mat3 basis = makeOrthonormalBasis(N);
			vec3 Vb = V * basis;
			vec3 H = normalize(basis * importanceSampleDGGXVNDF(reflectionSampleUV, linearRoughness, Vb));
			vec3 L = reflect(-V, H);

			float NoL = max(0.0f, dot(N, L));
			float NoH = max(0.0f, dot(N, H));
			float LoH = max(0.0f, dot(L, H));
			float VoH = max(0.0f, dot(V, H));

			float sD = D_GGX(linearRoughness, NoH);
			float sG = G1_Smith(linearRoughness, NoL);
			vec3 sF = F_Schlick(specularColor, 1.0f, LoH);

			throughput *= sF * sG;
			throughput /= specularProbability;

			primaryRay.direction = L;
			float denom = 4.0f * VoH;
			scatterPdfW = denom > 0.0f ? sD * NoH / denom : 0.0f;
		}
		else
		{
			throughput *= useIndirectSpecular ? diffuseColor : payload.baseColor;
			throughput /= (1.0f - specularProbability);

			primaryRay.direction = safeNormalize(N + mapToUniformSphere(reflectionSampleUV));
			scatterPdfW = 1.0f / M_PI;
		}
	}

	if (showFocalPlane)
	{
		result = mix(result, vec3(1.0f, 0.0f, 0.0f), focalOverlay);
	}

	ivec2 focusPick = PT_SCENE(ctx, focusPickPixel);
	if (focusPick.x >= 0 && pixelIndex.x == focusPick.x && pixelIndex.y == focusPick.y)
	{
		PT_FOCUS_WRITE(ctx, primaryDepth);
	}

	float frame = float(PT_SCENE(ctx, frameIndex));
	if (!skipAccum && frame > 0.0f)
	{
		result = mix(PT_OUTPUT_READ(ctx, pixelIndex), result, 1.0f / (frame + 1.0f));
	}
	PT_OUTPUT_WRITE(ctx, pixelIndex, result);
}

#endif // PT_HAS_RENDER_LOOP

#endif // INCLUDED_PATH_TRACER_CORE
