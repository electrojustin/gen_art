#include <QApplication>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <png.h>
#include <thread>

#include "qt_display.h"

int width;
int height;
uint8_t* buf;
QtDisplay* display;
std::thread* paint_thread;

struct TargetPixel {
  uint32_t color;
  int orig_x;
  int orig_y;
  int orig_z;
  int x;
  int y;
  int z;
};

std::vector<TargetPixel> target_pixels;

void read_png_file(const char* file_name, int& width, int& height, uint8_t*& buf) {
  unsigned char header[8];

  FILE *fd = fopen(file_name, "rb");
  if (!fd) {
    printf("Could not open file %s\n", file_name);
    exit(-1);
  }

  fread(header, 1, 8, fd);
  if (png_sig_cmp(header, 0, 8)) {
    printf("Could not validate png header\n");
    exit(-1);
  }

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr) {
    printf("Could not create png_ptr\n");
    exit(-1);
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    printf("Could not create info_ptr\n");
    exit(-1);
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    printf("Error during init_io\n");
    exit(-1);
  }

  png_init_io(png_ptr, fd);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);

  width = png_get_image_width(png_ptr, info_ptr);
  height = png_get_image_height(png_ptr, info_ptr);

  buf = (uint8_t*)malloc(height*png_get_rowbytes(png_ptr, info_ptr));
  png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
  uint8_t* tmp_buf = buf;
  for (int y = 0; y < height; y++) {
    row_pointers[y] = tmp_buf;
    tmp_buf += png_get_rowbytes(png_ptr, info_ptr);
  }

  png_read_image(png_ptr, row_pointers);

  free(row_pointers);
  fclose(fd);
}

void greyscale_image() {
  for (int i = 0; i < width*height; i++) {
    int r = buf[i*4];
    int g = buf[i*4+1];
    int b = buf[i*4+2];

    int y = 66*r + 129*g + 25*b;
    y = (y+128) >> 8;
    y = y + 16;
    if (y > 255)
      y = 255;
    if (y < 0)
      y = 0;

    buf[i*4] = y;
    buf[i*4+1] = y;
    buf[i*4+2] = y;
  }
}

uint32_t bit_reverse(int x, int n) {
  uint32_t ret = 0;
  for (int i = 0; i < n; i++) {
    ret |= (x & 0x01) << (n - 1 - i);
    x >>= 1;
  }

  return ret;
}

uint32_t bit_interleave(uint32_t x, uint32_t y, int n) {
  uint32_t ret = 0;
  for (int i = 0; i < n/2; i++) {
    ret |= (x & 0x1) << (2*i);
    x >>= 1;
    ret |= (y & 0x1) << (2*i+1);
    y >>= 1;
  }

  return ret;
}

int bayer_coefficient(int x, int y, int n) {
  return bit_reverse(bit_interleave(x^y, y, n), n);
}

void darken_foreground() {
  for (int i = 0; i < width*height; i++) {
    int y = buf[i*4];
    if (y < 200)
      y /= 3;

    buf[i*4] = y;
    buf[i*4+1] = y;
    buf[i*4+2] = y;
  }
}

void dither_image(int n) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      int y = buf[(j*width+i)*4];
      y += bayer_coefficient(i, j, 8-n);
      if (y > 255) {
        y = 255;
      } else {
        y >>= 8-n;
        y = (y*255) / ((1 << n)-1);
      }

      buf[(j*width+i)*4] = y & 0xFF;
      buf[(j*width+i)*4+1] = y & 0xFF;
      buf[(j*width+i)*4+2] = y & 0xFF;
    }
  }
}

void find_target_pixels() {
  uint32_t* color_buf = (uint32_t*)buf;
  for (int i = 0; i < width*height; i++) {
    if ((*color_buf & 0xFF) < 150) {
      TargetPixel pixel;
      pixel.color = *color_buf;
      pixel.orig_x = i % width;
      pixel.orig_y = i / width;
      pixel.orig_z = 0;
      pixel.x = pixel.orig_x;
      pixel.y = pixel.orig_y;
      pixel.z = pixel.orig_z;

      target_pixels.push_back(pixel);
    }

    color_buf++;
  }
}

void paint_target_pixels() {
  int kFadeCoeff = 10;
//  memset(buf, 255, width*height*4);

  for (int i = 0; i < width*height; i++) {
    int r = buf[i*4];
    int g = buf[i*4+1];
    int b = buf[i*4+2];

    r += kFadeCoeff;
    if (r > 255)
      r = 255;
    g += kFadeCoeff;
    if (g > 255)
      g = 255;
    b += kFadeCoeff;
    if (b > 255)
      b = 255;

    buf[i*4] = r;
    buf[i*4+1] = g;
    buf[i*4+2] = b;
  }

  uint32_t* color_buf = (uint32_t*)buf;

  for (auto pixel : target_pixels)
    color_buf[pixel.y*width + pixel.x] = pixel.color;
}

int restart_probability = 100;
int restart_probability_dir = -1;

void walk_targets() {
  std::vector<TargetPixel> new_targets;
  for (auto pixel : target_pixels) {
    if (rand() % 100 < restart_probability) {
      if (pixel.x > pixel.orig_x)
        pixel.x--;
      if (pixel.x < pixel.orig_x)
        pixel.x++;
      if (pixel.y > pixel.orig_y)
        pixel.y--;
      if (pixel.y < pixel.orig_y)
        pixel.y++;
//      pixel.x = pixel.orig_x;
//      pixel.y = pixel.orig_y;
    } else {
      switch (rand() % 4) {
        case 0:
          if (pixel.y > 0)
            pixel.y--;
          break;
        case 1:
          if (pixel.x < width)
            pixel.x++;
          break;
        case 2:
          if (pixel.y < height)
            pixel.y++;
          break;
        case 3:
          if (pixel.x > 0)
            pixel.x--;
          break;
        default:
          break;
      }
    }

    new_targets.push_back(pixel);
  }

  target_pixels = std::move(new_targets);
}

void paint_loop() {
  auto last_buf_swap = std::chrono::high_resolution_clock::now();
  int kRefreshPeriod = 33000;
  uint64_t frame_count = 0;
  while(1) {
    if (frame_count % 5 == 0) {
      restart_probability += restart_probability_dir;
      if (restart_probability < 0) {
        restart_probability = 0;
        restart_probability_dir *= -1;
      } else if (restart_probability > 100) {
        restart_probability = 100;
        restart_probability_dir *= -1;
      }
    }

    frame_count++;

    walk_targets();
    paint_target_pixels();
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

  read_png_file(argv[1], width, height, buf);

  greyscale_image();
  //darken_foreground();
  dither_image(1);

  find_target_pixels();

  QApplication app(argc, argv);

  display = new QtDisplay(width, height);

  paint_thread = new std::thread(paint_loop);

  return app.exec();
}
