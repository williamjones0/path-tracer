#version 460 core

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

struct Material {
    vec4 albedo;
    uint type;
};

#define MATERIAL_LIGHT 0
#define MATERIAL_LAMBERTIAN 1
#define MATERIAL_METAL 2
#define MATERIAL_GLASS 3

struct quad {
	vec4 Q;
	vec4 u;
	vec4 v;
	uint materialIndex;
};

struct light {
	uint quadIndex;
	float area;
};

layout (binding = 0, rgba32f) uniform image2D outputImage;

const float pi = 3.1415926535897932385;

uniform vec3 camPos;
uniform vec3 camDir;
uniform vec3 w;
uniform vec3 u;
uniform vec3 v;
uniform vec3 pixel00_loc;
uniform vec3 pixel_delta_u;
uniform vec3 pixel_delta_v;
uniform vec3 defocus_disk_u;
uniform vec3 defocus_disk_v;
uniform float defocus_angle;
uniform float focus_dist;

layout (binding = 1, std430) readonly buffer Quads {
	quad quads_[];
};

layout (binding = 2, std430) readonly buffer Lights {
	light lights[];
};

layout (binding = 3, std430) readonly buffer Materials {
	Material materials[];
};

const float vFov = 40.0;

const int MAX_DEPTH = 1;

const int SAMPLES_PER_PIXEL = 5;
const int sqrt_spp = int(sqrt(SAMPLES_PER_PIXEL));
const float pixel_samples_scale = 1.0 / (sqrt_spp * sqrt_spp);
const float recip_sqrt_spp = 1.0 / sqrt_spp;

const vec3 BACKGROUND_COLOR = vec3(0.0, 0.0, 0.0);
const int NUM_TRIANGLES = 6;

uniform int IMAGE_HEIGHT;
uniform int IMAGE_WIDTH;
float ASPECT_RATIO = IMAGE_WIDTH / IMAGE_HEIGHT;

struct Camera {
	float defocus_angle;
	float focus_dist;

	vec3 center;
	vec3 pixel00_loc;
	vec3 pixel_delta_u;
	vec3 pixel_delta_v;
	vec3 u, v, w;
	vec3 defocus_disk_u;
	vec3 defocus_disk_v;
};

struct ray {
	vec3 orig;
	vec3 dir;
	float tm;
};

struct hit_record {
	vec3 p;
	vec3 normal;
	uint materialIndex;
	float t;
	float u;
	float v;
	int back_face;

	// PDF of scattering in the given direction from material properties
	float scatterPdf;
	// PDF of sampling in the given direction
	float samplePdf;
};

struct onb {
    vec3 u;
    vec3 v;
    vec3 w;
};

onb Onb(vec3 n) {
    onb res;
    res.w = normalize(n);
    vec3 a = (abs(res.w.x) > 0.9) ? vec3(0,1,0) : vec3(1,0,0);
    res.v = normalize(cross(res.w, a));
    res.u = cross(res.w, res.v);
    return res;
}

vec3 onbLocal(vec3 a, onb o) {
    return a.x * o.u + a.y * o.v + a.z * o.w;
}

// UTILITY FUNCTIONS

// General
uint rng_state;

uint rand_pcg() {
    uint state = rng_state;
    rng_state = rng_state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random_float() {
	return float(rand_pcg()) / 4294967296.0;
}

float random_float(float mn, float mx) {
	return mn + (mx - mn) * random_float();
}

int random_int(int mn, int mx) {
	return int(random_float(mn, mx + 1));
}

// Vec3
vec3 random_in_unit_disk() {
	vec3 p;
	do {
		p = 2.0 * vec3(random_float(-1, 1), random_float(-1, 1), 0);
	} while (dot(p, p) >= 1.0);
	return p;
}

vec3 random_in_unit_sphere() {
	vec3 p;
	do {
		p = 2.0 * vec3(random_float(-1, 1), random_float(-1, 1), random_float(-1, 1));
	} while (dot(p, p) >= 1.0);
	return p;
}

vec3 random_unit_vector() {
	return normalize(random_in_unit_sphere());
}

vec3 random_on_hemisphere(vec3 normal) {
	vec3 on_unit_sphere = random_unit_vector();
	if (dot(on_unit_sphere, normal) > 0.0) {
		return on_unit_sphere;
	} else {
		return -on_unit_sphere;
	}
}

vec3 random_cosine_direction() {
	float r1 = random_float();
	float r2 = random_float();

	float phi = 2 * pi * r1;
	float x = cos(phi) * sqrt(r2);
	float y = sin(phi) * sqrt(r2);
	float z = sqrt(1 - r2);

	return vec3(x, y, z);
}

// Camera
vec3 sample_square_stratified(int s_i, int s_j) {
	float px = ((s_i + random_float()) * recip_sqrt_spp) - 0.5;
	float py = ((s_j + random_float()) * recip_sqrt_spp) - 0.5;

	return vec3(px, py, 0);
}

vec3 sample_square() {
	return vec3(random_float() - 0.5, random_float() - 0.5, 0);
}

vec3 defocus_disk_sample(Camera cam) {
	vec3 p = random_in_unit_disk();
	return cam.center + p.x * cam.defocus_disk_u + p.y * cam.defocus_disk_v;
}

Camera initialiseCamera() {
	Camera cam;

	cam.defocus_angle = defocus_angle;
	cam.focus_dist = focus_dist;

	cam.center = camPos;

	cam.w = w;
	cam.u = u;
	cam.v = v;

	cam.pixel00_loc = pixel00_loc;
	cam.pixel_delta_u = pixel_delta_u;
	cam.pixel_delta_v = pixel_delta_v;

	cam.defocus_disk_u = defocus_disk_u;
	cam.defocus_disk_v = defocus_disk_v;

	return cam;
}

ray get_ray(Camera cam, int i, int j, int s_i, int s_j) {
	vec3 offset = sample_square_stratified(s_i, s_j);
	offset = vec3(0, 0, 0);
	vec3 pixel_sample = cam.pixel00_loc
	                  + (i + offset.x) * cam.pixel_delta_u
					  + (j + offset.y) * cam.pixel_delta_v;

	vec3 ray_origin = (cam.defocus_angle <= 0) ? cam.center : defocus_disk_sample(cam);
	vec3 ray_direction = pixel_sample - ray_origin;
	float ray_time = random_float();

	return ray(ray_origin, ray_direction, ray_time);
}

vec3 sampleLambertian(vec3 normal, inout float pdf) {
	onb uvw = Onb(normal);
	vec3 random_cos = random_cosine_direction();
	vec3 random_cos_scattered = normalize(onbLocal(random_cos, uvw));
	float cosine = dot(normalize(normal), random_cos_scattered);
	pdf = max(0.01, cosine) / pi;

	return random_cos_scattered;
}

vec3 sampleMetal(vec3 I, vec3 normal, inout float pdf) {
	pdf = 1.0;
	return reflect(I, normal);
}

vec3 sampleGlass(vec3 I, hit_record rec, inout float pdf) {
	pdf = 1.0;
	float ir = 1.5;
	float refraction_ratio = (1.0 - rec.back_face) * (1.0 / ir) + rec.back_face * ir;
	vec3 i = normalize(I);
	float cos_theta = min(dot(-i, rec.normal), 1.0);
	float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	float t = floor(clamp(refraction_ratio * sin_theta, 0.0, 1.0));

	return t * reflect(i, rec.normal) + (1.0 - t) * refract(i, rec.normal, refraction_ratio);
}

vec3 randomOnAQuad(uint quadIndex) {
	float u = random_float();
	float v = random_float();
	quad q = quads_[quadIndex];
	vec3 Q = q.Q.xyz;
	vec3 u_ = q.u.xyz;
	vec3 v_ = q.v.xyz;
	return Q + u * u_ + v * v_;
};

vec3 sampleLight(vec3 p, inout float pdf, inout float lightCosine) {
	int lightIndex = int(floor(lights.length() * random_float()));
	vec3 onLight = randomOnAQuad(lights[lightIndex].quadIndex);
	vec3 toLight = onLight - p;
	float distSquared = dot(toLight, toLight);
	toLight = normalize(toLight);
	lightCosine = abs(toLight.y);

	pdf = distSquared / (lights.length() * lightCosine * lights[lightIndex].area);

	return toLight;
}

bool scatter(ray r_in, inout hit_record rec, inout vec3 albedo, inout ray scattered) {
	albedo = materials[rec.materialIndex].albedo.xyz;

	float materialSamplePdf;
	vec3 materialSample;

	if (materials[rec.materialIndex].type == MATERIAL_LAMBERTIAN) {
		materialSample = sampleLambertian(rec.normal, materialSamplePdf);
	} else if (materials[rec.materialIndex].type == MATERIAL_METAL) {
		materialSample = sampleMetal(r_in.dir, rec.normal, materialSamplePdf);
	} else if (materials[rec.materialIndex].type == MATERIAL_GLASS) {
		materialSample = sampleGlass(r_in.dir, rec, materialSamplePdf);
		albedo = vec3(1.0);
	}

	float lightSamplePdf = materialSamplePdf;
	vec3 finalSample = materialSample;

	if (random_float() < 0.5 && materials[rec.materialIndex].type == MATERIAL_LAMBERTIAN) {
		float lightCosine;
		finalSample = sampleLight(rec.p, lightSamplePdf, lightCosine);

		if (abs(lightCosine) < 0.001) {
			finalSample = materialSample;
			lightSamplePdf = materialSamplePdf;
		}
	}

	scattered = ray(rec.p, finalSample, r_in.tm);

	rec.samplePdf = 0.5 * materialSamplePdf + 0.5 * lightSamplePdf;
	rec.scatterPdf = materialSamplePdf;

	return materials[rec.materialIndex].type == MATERIAL_LIGHT;
}

//vec3 triIntersect(in vec3 ro, in vec3 rd, triangle tri, inout vec3 n) {
//    vec3 a = tri.v0 - tri.v1;
//    vec3 b = tri.v2 - tri.v0;
//    vec3 p = tri.v0 - ro;
//    n = cross(b, a);
//
//    vec3 q = cross(p, rd);
//
//    float idet = 1.0 / dot(rd, n);
//
//    float u = dot(q, b) * idet;
//    float v = dot(q, a) * idet;
//    float t = dot(n, p) * idet;
//
//    return vec3(t, u, v);
//}
//
//bool hit_triangle(int triangleIndex, ray r, float tMin, float tMax, inout hit_record rec) {
//	triangle t = triangles[triangleIndex];
//	vec3 normal = vec3(0.0);
//	vec3 hit = triIntersect(r.orig, r.dir, t, normal);
//	if (!(hit.y < 0.0 || hit.y > 1.0 || hit.z < 0.0 || (hit.y + hit.z) > 1.0)) {
//		rec.p = r.orig + hit.x * r.dir;
//		rec.normal = normalize(normal);
//		rec.back_face = dot(r.dir, rec.normal) > 0 ? 1 : 0;
//		rec.normal *= 1.0 - 2.0 * rec.back_face;
//		rec.p += rec.normal * 0.0001;
//		rec.t = hit.x;
//		rec.materialIndex = t.materialIndex;
//
//		return hit.x > tMin && hit.x < tMax;
//	}
//
//	return false;
//}

bool is_interior(float a, float b, inout hit_record rec) {
	if (!(0 <= a && a <= 1) || !(0 <= b && b <= 1)) {
		return false;
	}

	rec.u = a;
	rec.v = b;
	return true;
}

bool hit_quad(int quadIndex, ray r, float tMin, float tMax, inout hit_record rec) {
	quad q = quads_[quadIndex];

	// Initialise vec3s
	vec3 Q = q.Q.xyz;
	vec3 u = q.u.xyz;
	vec3 v = q.v.xyz;
	
	vec3 n = cross(u, v);
	vec3 normal = normalize(n);
	float D = dot(normal, Q);
	vec3 w = n / dot(n, n);

	float denom = dot(normal, r.dir);

	if (abs(denom) < 1e-8) {
		return false;
	}

	float t = (D - dot(normal, r.orig)) / denom;
	if (t < tMin || t > tMax) {
		return false;
	}

	vec3 intersection = r.orig + t * r.dir;
	vec3 planar_hitpt_vector = intersection - Q;
	float alpha = dot(w, cross(planar_hitpt_vector, v));
	float beta = dot(w, cross(u, planar_hitpt_vector));

	if (!is_interior(alpha, beta, rec)) {
		return false;
	}

	rec.t = t;
	rec.p = intersection;
	rec.materialIndex = q.materialIndex;

	rec.back_face = dot(r.dir, normal) > 0 ? 1 : 0;
	rec.normal = rec.back_face == 1 ? -normal : normal;
	return true;
}

bool hit_scene(ray r, inout hit_record rec) {
	float t_min = 0.001;
	float t_max = 10000.0;

	hit_record temp_rec;
	bool hit_anything = false;
	float closest_so_far = t_max;
	for (int i = 0; i < NUM_TRIANGLES; ++i) {
		if (hit_quad(i, r, t_min, closest_so_far, temp_rec)) {
			hit_anything = true;
			closest_so_far = temp_rec.t;
			rec = temp_rec;
		}
	}

	return hit_anything;
}

vec2 intersectAABB(ray r, vec3 mn, vec3 mx) {
	vec3 tMin = (mn - r.orig) / r.dir;
	vec3 tMax = (mx - r.orig) / r.dir;
	vec3 t0 = min(tMin, tMax);
	vec3 t1 = max(tMin, tMax);
	float tNear = max(max(t0.x, t0.y), t0.z);
	float tFar = min(min(t1.x, t1.y), t1.z);
	return vec2(tNear, tFar);
}

vec3 ray_color(ray r) {
	hit_record rec;

	vec3 color = vec3(1.0);
	ray current_ray = { r.orig, r.dir, r.tm };

	for (int i = 0; i < MAX_DEPTH; ++i) {
		if (!hit_scene(current_ray, rec)) {
			color *= BACKGROUND_COLOR;
			break;
		}

		vec3 albedo;
		bool emits = scatter(current_ray, rec, albedo, current_ray);
		color *= albedo;
		if (emits) {
			break;
		}
	}

	return color;
}

void main() {
	rng_state = uint(gl_GlobalInvocationID.x * 1000 + gl_GlobalInvocationID.y);

	Camera cam = initialiseCamera();

	int i = int(gl_GlobalInvocationID.x);
	int j = IMAGE_HEIGHT - 1 - int(gl_GlobalInvocationID.y);

	vec3 color = vec3(0.0);
	for (int s_j = 0; s_j < sqrt_spp; s_j++) {
		for (int s_i = 0; s_i < sqrt_spp; s_i++) {
			ray r = get_ray(cam, i, j, s_i, s_j);
			color += ray_color(r);
		}
	}

	color *= pixel_samples_scale;

	imageStore(outputImage, ivec2(i, IMAGE_HEIGHT - 1 - j), vec4(color, 1.0));
}
