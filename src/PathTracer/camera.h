#pragma once

#include <chrono>

#include "util.h"

#include "hittable.h"
#include "pdf.h"
#include "material.h"

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "include/stb_image_write.h"

void write_image(const char *filename, int width, int height, int channels, unsigned char *data) {
	stbi_write_png(filename, width, height, channels, data, width * channels);
}

class camera {
public:
	double aspect_ratio = 1.0;
	int image_width = 100;
	int samples_per_pixel = 10;
	int max_depth = 10;
	color background;

	double vfov = 90;
	point3 lookfrom = point3(0, 0, 0);
	point3 lookat = point3(0, 0, -1);
	vec3 vup = vec3(0, 1, 0);

	double defocus_angle = 0;
	double focus_dist = 10;

	int image_height;
	double pixel_samples_scale;
	int sqrt_spp;
	double recip_sqrt_spp;
	point3 center;
	point3 pixel00_loc;
	vec3 pixel_delta_u;
	vec3 pixel_delta_v;
	vec3 u, v, w;
	vec3 defocus_disk_u;
	vec3 defocus_disk_v;

	unsigned char *imageData;

	void render(const hittable &world, const hittable &lights) {
		auto start = std::chrono::high_resolution_clock::now();

		initialize();

		for (int j = 0; j < image_height; j++) {
			std::clog << "\rScanlines remaining: " << image_height - j << ' ' << std::flush;
			for (int i = 0; i < image_width; i++) {
				color pixel_color(0, 0, 0);
				for (int s_j = 0; s_j < sqrt_spp; s_j++) {
					for (int s_i = 0; s_i < sqrt_spp; s_i++) {
						ray r = get_ray(i, j, s_i, s_j);
						pixel_color += ray_color(r, max_depth, world, lights);
					}
				}
				write_color(imageData, i, j, image_width, image_height, pixel_samples_scale * pixel_color);
			}
		}

		std::clog << "\rDone.                 \n";

		write_image("output.png", image_width, image_height, 3, imageData);

		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end - start;
		std::clog << "Elapsed time: " << elapsed.count() << "s\n";
	}

	void initialize() {
		image_height = static_cast<int>(image_width / aspect_ratio);
		image_height = (image_height < 1) ? 1 : image_height;

		imageData = new unsigned char[image_width * image_height * 3];

		calculateParameters();
	}

	void calculateParameters() {
		sqrt_spp = static_cast<int>(std::sqrt(samples_per_pixel));
		pixel_samples_scale = 1.0 / (sqrt_spp * sqrt_spp);
		recip_sqrt_spp = 1.0 / sqrt_spp;

		center = lookfrom;

		auto theta = degrees_to_radians(vfov);
		auto h = std::tan(theta / 2);
		auto viewport_height = 2 * h * focus_dist;
		auto viewport_width = viewport_height * static_cast<double>(image_width) / image_height;

		w = unit_vector(lookfrom - lookat);
		u = unit_vector(cross(vup, w));
		v = cross(w, u);

		auto viewport_u = viewport_width * u;
		auto viewport_v = viewport_height * -v;

		pixel_delta_u = viewport_u / image_width;
		pixel_delta_v = viewport_v / image_height;

		auto viewport_upper_left = center - (focus_dist * w) - 0.5 * (viewport_u + viewport_v);
		pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

		auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
		defocus_disk_u = defocus_radius * u;
		defocus_disk_v = defocus_radius * v;
	}

private:
	ray get_ray(int i, int j, int s_i, int s_j) const {
		auto offset = sample_square_stratified(s_i, s_j);
		auto pixel_sample = pixel00_loc
			              + (i + offset.x()) * pixel_delta_u
			              + (j + offset.y()) * pixel_delta_v;

		auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
		auto ray_direction = pixel_sample - ray_origin;
		auto ray_time = random_double();

		return ray(ray_origin, ray_direction, ray_time);
	}

	vec3 sample_square_stratified(int s_i, int s_j) const {
		auto px = ((s_i + random_double()) * recip_sqrt_spp) - 0.5;
		auto py = ((s_j + random_double()) * recip_sqrt_spp) - 0.5;

		return vec3(px, py, 0);
	}

	vec3 sample_square() const {
		return vec3(random_double() - 0.5, random_double() - 0.5, 0);
	}

	point3 defocus_disk_sample() const {
		auto p = random_in_unit_disk();
		return center + p.x() * defocus_disk_u + p.y() * defocus_disk_v;
	}

	color ray_color(const ray &r, int depth, const hittable &world, const hittable &lights) const {
		if (depth <= 0)
			return color(0, 0, 0);

		hit_record rec;

		if (!world.hit(r, interval(0.001, infinity), rec))
			return background;

		scatter_record srec;
		color color_from_emission = rec.mat->emitted(r, rec, rec.u, rec.v, rec.p);

		if (!rec.mat->scatter(r, rec, srec))
			return color_from_emission;

		if (srec.skip_pdf)
			return srec.attenuation * ray_color(srec.skip_pdf_ray, depth - 1, world, lights);

		auto light_ptr = make_shared<hittable_pdf>(lights, rec.p);
		mixture_pdf p(light_ptr, srec.pdf_ptr);

		ray scattered = ray(rec.p, p.generate(), r.time());
		auto pdf_value = p.value(scattered.direction());

		double scattering_pdf = rec.mat->scattering_pdf(r, rec, scattered);

		color sample_color = ray_color(scattered, depth - 1, world, lights);
		color color_from_scatter =
			(srec.attenuation * scattering_pdf * sample_color) / pdf_value;

		return color_from_emission + color_from_scatter;
	}
};
