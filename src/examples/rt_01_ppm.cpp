//
// Created by User on 2026/3/16.
//

#include "rt_01_ppm.h"
#include <fstream>
#include <iostream>
#include <string>
#include "vec3.h"
#include "color.h"

int main(int argc, char* argv[]) {
    const int image_width = 256;
    const int image_height = 256;
    const std::string output_path = argc > 1 ? argv[1] : "image.ppm";

    std::ofstream out(output_path, std::ios::out | std::ios::trunc);
    if (!out) {
        std::cerr << "Failed to open output file: " << output_path << '\n';
        return 1;
    }

    out << "P3\n" << image_width << " " << image_height << "\n255\n";

    for (int j = 0; j < image_height; j++) {
        std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
        for (int i = 0; i < image_width; i++) {
            auto pixel_color = color(double(i)/(image_width-1), double(j)/(image_height-1), 0);
            write_color(std::cout, pixel_color);
        }
    }

    std::cout << "Wrote " << output_path << '\n';
    std::clog << "\rDone.       \n";
    return 0;
}
