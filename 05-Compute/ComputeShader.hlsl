cbuffer Constants : register(b0, space0)
{
	float g_elapsedTime;
	float g_padding0;
	float g_padding1;
	float g_padding2;
};

[[vk::image_format("rgba8")]] RWTexture2D<float4> outputImage : register(u1, space0);

#define ID_ENV 0
#define ID_SPHERE 1
#define ID_PLANE 2
#define far_dist 1000.0f

struct ray
{
	float3 o;
	float3 d;
};

struct sphere
{
	float3 c;
	float r;
};

struct plane
{
	float3 n;
	float d;
};

struct intersection
{
	float3 n;
	float3 p;
	float t;
};

struct surface
{
	int id;
	float3 p;
	float3 n;
	float3 e;
	float3 a;
};

intersection ray_plane(ray r, plane p)
{
	intersection res;

	res.t = far_dist;

	float t = (p.d - dot(r.o, p.n)) / dot(r.d, p.n);

	if (t < 0.0f)
	{
		return res;
	}

	res.p = r.o + r.d * t;
	res.t = t;
	res.n = p.n;

	return res;
}

intersection ray_sphere(ray r, sphere s)
{
	intersection res;
	res.t = far_dist;

	float3 local_ro = r.o - s.c;

	float b = 2.0f * dot(local_ro, r.d);
	float c = dot(local_ro, local_ro) - s.r * s.r;
	float d = b * b - 4.0f * c;

	if (d < 0.0f)
	{
		return res;
	}

	float t = (-b - sqrt(d)) / 2.0f;
	if (t < 0.0f)
	{
		return res;
	}

	res.t = t;
	res.p = r.o + r.d * res.t;
	res.n = normalize(res.p - s.c);

	return res;
}

float3x3 look_at(float3 p, float3 t)
{
	float3 d = normalize(t - p);

	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 right = normalize(cross(up, d));
	up = cross(d, right);

	return float3x3(right, up, d);
}

float3 sampleEnvironment(float3 d)
{
	return 0.25f * float3(d * 0.5f + 0.5f).bgr;
}

float3 sampleGround(float2 p)
{
	p *= 5.0f;
	return float3(frac(p.x), frac(p.y), 0.0f);
}

surface raytrace(ray r, float elapsedTime)
{
	surface res;
	res.id = ID_ENV;
	res.p = r.o + r.d * far_dist;
	res.n = -r.d;
	res.e = r.d;
	res.a = sampleEnvironment(r.d);

	float t = far_dist;

	intersection i;

	{
		sphere s;
		s.c = 1.25f * float3(cos(elapsedTime * 2.0f), 0.0f, sin(elapsedTime * 2.0f));
		s.r = 0.25f;
		i = ray_sphere(r, s);
		if (i.t < t)
		{
			res.id = ID_SPHERE;
			res.p = i.p;
			res.n = i.n;
			res.e = r.d;
			res.a = float3(0.1f, 1.0f, 0.2f);
			t = i.t;
		}
	}

	{
		sphere s;
		s.c = -1.25f * float3(cos(elapsedTime * 2.0f), 0.0f, sin(elapsedTime * 2.0f));
		s.r = 0.25f;
		i = ray_sphere(r, s);
		if (i.t < t)
		{
			res.id = ID_SPHERE;
			res.p = i.p;
			res.n = i.n;
			res.e = r.d;
			res.a = float3(1.0f, 0.2f, 0.1f);
			t = i.t;
		}
	}

	{
		sphere s;
		s.c = float3(0.0f, 0.0f, 0.0f);
		s.r = 1.0f;
		i = ray_sphere(r, s);
		if (i.t < t)
		{
			res.id = ID_SPHERE;
			res.p = i.p;
			res.n = i.n;
			res.e = r.d;
			res.a = float3(1.0f, 1.0f, 1.0f);
			t = i.t;
		}
	}

	{
		plane p;
		p.n = float3(0.0f, 1.0f, 0.0f);
		p.d = -1.0f;
		i = ray_plane(r, p);
		if (i.t < t)
		{
			res.id = ID_PLANE;
			res.p = i.p;
			res.n = i.n;
			res.e = r.d;
			res.a = sampleGround(i.p.xz * 0.25f);
			t = i.t;
		}
	}

	return res;
}

float3 evaluate(surface surf)
{
	return surf.a;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint2 outputSize;
	outputImage.GetDimensions(outputSize.x, outputSize.y);

	float2 resolution = float2(outputSize);
	float2 uv = float2(dispatchThreadId.xy) / resolution;
	uv.y = 1.0f - uv.y;

	float3 res = float3(0.0f, 0.0f, 0.0f);

	ray r;
	r.o = float3(2.0f * cos(15.0f + g_elapsedTime * 0.2f),
	           0.25f + sin(5.0f + g_elapsedTime * 0.05f),
	           4.0f * sin(g_elapsedTime * 0.1f));
	r.d = float3(uv * 2.0f - 1.0f, 1.0f);
	r.d.x *= resolution.x / resolution.y;
	r.d = mul(normalize(r.d), look_at(r.o, float3(0.0f, 0.0f, 0.0f)));

	int num_bounces = 0;
	const int max_bounces = 3;
	surface surfaces[max_bounces];

	for (int i = 0; i < max_bounces; ++i)
	{
		++num_bounces;
		surface surf = raytrace(r, g_elapsedTime);
		surfaces[i] = surf;
		if (surf.id == ID_ENV)
		{
			break;
		}
		r.d = reflect(surf.e, surf.n);
		r.o = surf.p + r.d * 0.001f;
	}

	for (int i = max_bounces - 1; i >= 0; --i)
	{
		float dp = clamp(dot(-surfaces[i].e, surfaces[i].n), 0.0f, 1.0f);
		float f = pow(1.0f - dp, 4.0f);
		res = lerp(evaluate(surfaces[i]), res, f);
	}

	outputImage[dispatchThreadId.xy] = float4(res, 1.0f);
}
