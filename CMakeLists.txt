cmake_minimum_required(VERSION 3.15)

project(PathTracer)

add_executable(PathTracer main.cpp "vec3.h" "color.h" "ray.h" "hittable.h" "sphere.h" "hittable_list.h" "util.h" "interval.h" "camera.h" "material.h")