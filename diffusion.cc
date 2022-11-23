#include <QApplication>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <png.h>
#include <thread>

#include "qt_display.h"

int width = 500;
int height = 500;
uint8_t* buf;
QtDisplay* display;
std::thread* paint_thread;
double* concentration;
double* grad_x;
double* grad_y;
double* double_grad_x;
double* double_grad_y;
double* laplacian;

void compute_x_grad(double* vals, double* out_x) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      double prev_x, next_x;

      if (x == 0) {
        prev_x = 0;
      } else {
        prev_x = vals[y*width + x - 1];
      }
      if (x == width-1) {
        next_x = 0;
      } else {
        next_x = vals[y*width + x + 1];
      }

      out_x[y*width + x] = next_x - prev_x;
    }
  }
}

void compute_y_grad(double* vals, double* out_y) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      double prev_y, next_y;

      if (y == 0) {
        prev_y = 0;
      } else {
        prev_y = vals[(y-1)*width + x];
      }
      if (y == width-1) {
        next_y = 0;
      } else {
        next_y = vals[(y+1)*width + x];
      }

      out_y[y*width + x] = next_y - prev_y;
    }
  }
}

void compute_laplacian() {
  compute_x_grad(concentration, grad_x);
  compute_x_grad(grad_x, double_grad_x);
  compute_y_grad(concentration, grad_y);
  compute_y_grad(grad_y, double_grad_y);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      laplacian[y*width + x] = double_grad_x[y*width + x] + double_grad_y[y*width + x];
    }
  }
}

void process() {
  double diffusion_coefficient = 0.1;
  double step = 1.0;

  compute_laplacian();

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      concentration[y*width + x] += step*(diffusion_coefficient*laplacian[y*width+x]);
    }
  }
}

uint64_t frame = 0;

void seed() {
  if (frame++ > 100)
    return;
  int seed_x = 50;
  int seed_y = 50;
  concentration[seed_y*width + seed_x] = 10.0;
  concentration[seed_y*width + seed_x+1] = 10.0;
  concentration[(seed_y+1)*width + seed_x] = 10.0;
  concentration[(seed_y+1)*width + seed_x+1] = 10.0;
}

void render() {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      double y_val = concentration[y*width + x] * 255.0;
      if (y_val > 255.0)
        y_val = 255.0;
      if (y_val < 0.0)
        y_val = 0;

      buf[4*(y*width+x)] = y_val;
      buf[4*(y*width+x)+1] = y_val;
      buf[4*(y*width+x)+2] = y_val;
      buf[4*(y*width+x)+3] = 255;
    }
  }
}

void paint_loop() {
  auto last_buf_swap = std::chrono::high_resolution_clock::now();
  int kRefreshPeriod = 33000;
  uint64_t frame_count = 0;
  seed();
  while(1) {
    process();
    seed();
    render();

    display->swap_buf(buf);

    auto curr_time = std::chrono::high_resolution_clock::now();
    auto time_in_microseconds =
        std::chrono::duration_cast<std::chrono::microseconds>(curr_time -
                                                              last_buf_swap);

    if (time_in_microseconds.count() < kRefreshPeriod) {
      usleep(kRefreshPeriod - time_in_microseconds.count());
    } else {
      printf("Warning! Frame lag! %lu us\n", time_in_microseconds.count());
    }
    last_buf_swap = std::chrono::high_resolution_clock::now();
  }
}

int main(int argc, char** argv) {
  time_t t;

  srand((unsigned) time(&t));

  buf = (uint8_t*)malloc(width*height*4);
  concentration = (double*)malloc(width*height*sizeof(double));
  grad_x = (double*)malloc(width*height*sizeof(double));
  grad_y = (double*)malloc(width*height*sizeof(double));
  double_grad_x = (double*)malloc(width*height*sizeof(double));
  double_grad_y = (double*)malloc(width*height*sizeof(double));
  laplacian = (double*)malloc(width*height*sizeof(double));

  QApplication app(argc, argv);

  display = new QtDisplay(width, height);

  paint_thread = new std::thread(paint_loop);

  return app.exec();
}
