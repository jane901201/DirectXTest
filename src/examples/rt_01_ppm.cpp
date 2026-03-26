//
// Created by User on 2026/3/16.
//

#include "rt_01_ppm.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "vec3.h"
#include "color.h"
#include "ray.h"

color ray_color(const ray& r) {
    vec3 unit_direction = unit_vector(r.direction());
    auto a = 0.5*(unit_direction.y() + 1.0);
    return (1.0-a)*color(1.0, 1.0, 1.0) + a*color(0.5, 0.7, 1.0);
}

int main(int argc, char* argv[]) {

    //Image
    auto aspect_ratio = 16.0 / 9.0;
    const int image_width = 400;
    int image_height = int(image_width / aspect_ratio);

    //Camera
    auto focal_length = 1.0;
    auto viewport_height = 2.0;
    auto viewport_width = viewport_height * (double(image_width)/image_height);
    auto camera_center = point3(0, 0, 0);

    //Calculate the vectors across the horizontal and down the vertical viewport edges.
    auto viewport_u = vec3(viewport_width, 0, 0);
    auto viewport_v = vec3(0, -viewport_height, 0);

    //Calculate the horizontal and vertical delta vectors from pixel to pixel.
    auto pixel_delta_u = viewport_u / image_width;
    auto pixel_delta_v = viewport_v / image_height;

    //Calculate the location of the upper left pixel.
    auto viewport_upper_left = camera_center - vec3(0, 0, focal_length) - viewport_u/2 - viewport_v/2;
    auto pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);


    const std::filesystem::path output_path = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path("image.ppm");

    std::ofstream out(output_path, std::ios::out | std::ios::trunc);
    if (!out) {
        std::cerr << "Failed to open output file: " << output_path << '\n';
        return 1;
    }

    out << "P3\n" << image_width << " " << image_height << "\n255\n";

    for (int j = 0; j < image_height; j++) {
        std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
        for (int i = 0; i < image_width; i++) {
            auto pixel_center = pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
            auto ray_direction = pixel_center - camera_center;
            ray r(camera_center, ray_direction);

            color pixel_color = ray_color(r);
            write_color(out, pixel_color);
        }
    }

    std::cout << "Wrote " << std::filesystem::absolute(output_path).string() << '\n';
    std::clog << "\rDone.       \n";
    return 0;
}
