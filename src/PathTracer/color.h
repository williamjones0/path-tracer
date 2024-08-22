#pragma once

#include "interval.h"
#include "vec3.h"

using color = vec3;

inline double linear_to_gamma(double linear_component) {
	if (linear_component > 0)
		return std::sqrt(linear_component);

	return 0;
}

void write_color(unsigned char *imageData, int i, int j, int image_width, int image_height, color pixel_color) {
	auto r = pixel_color.x();
	auto g = pixel_color.y();
	auto b = pixel_color.z();

	if (r != r) r = 0.0;
	if (g != g) g = 0.0;
	if (b != b) b = 0.0;

	r = linear_to_gamma(r);
	g = linear_to_gamma(g);
	b = linear_to_gamma(b);

	static const interval intensity(0.000, 0.999);
	int rbyte = static_cast<int>(255.999 * intensity.clamp(r));
	int gbyte = static_cast<int>(255.999 * intensity.clamp(g));
	int bbyte = static_cast<int>(255.999 * intensity.clamp(b));
	
	imageData[(j * image_width + i) * 3 + 0] = rbyte;
	imageData[(j * image_width + i) * 3 + 1] = gbyte;
	imageData[(j * image_width + i) * 3 + 2] = bbyte;
}
