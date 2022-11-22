#include <QApplication>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <png.h>
#include <thread>
#include <unordered_map>
#include <fftw3.h>

#include "qt_display.h"

int width;
int height;
uint8_t* buf;
double* greyscale_buf;
double* dct_buf;
double* dct_filtered_buf;
double* idct_buf;
fftw_plan idct_plan;
double* filter;
QtDisplay* display;
std::thread* paint_thread;

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

  buf = (uint8_t*)malloc(height*width*4);
  png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
  uint8_t* tmp_buf = buf;
  for (int y = 0; y < height; y++) {
    row_pointers[y] = tmp_buf;
    tmp_buf += png_get_rowbytes(png_ptr, info_ptr);
  }

  png_read_image(png_ptr, row_pointers);

  free(row_pointers);
  fclose(fd);

  if (png_get_rowbytes(png_ptr, info_ptr) == width*3) {
    uint8_t* tmp_buf = buf;
    buf = (uint8_t*)malloc(height*width*4);
    for (int i = 0; i < width*height; i++) {
      buf[i*4] = tmp_buf[i*3];
      buf[i*4+1] = tmp_buf[i*3+1];
      buf[i*4+2] = tmp_buf[i*3+2];
      buf[i*4+3] = 255;
    }
    free(tmp_buf);
  }
}

void setup() {
  greyscale_buf = (double*)fftw_malloc(sizeof(double)*width*height);
  dct_buf = (double*)fftw_malloc(sizeof(double)*width*height);
  dct_filtered_buf = (double*)fftw_malloc(sizeof(double)*width*height);
  idct_buf = (double*)fftw_malloc(sizeof(double)*width*height);
  filter = (double*)fftw_malloc(sizeof(double)*width*height);

  double* tmp_ret = greyscale_buf;
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

    *(tmp_ret++) = (double)y;
  }

  fftw_plan p;
  p = fftw_plan_r2r_2d(width, height, greyscale_buf, dct_buf, FFTW_REDFT10, FFTW_REDFT10, FFTW_ESTIMATE);
  fftw_execute(p);

  idct_plan = fftw_plan_r2r_2d(width, height, dct_filtered_buf, idct_buf, FFTW_REDFT01, FFTW_REDFT01, FFTW_ESTIMATE);

  filter[0] = 1.0;
}

void create_bandpass(int start, int end) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      if ((x >= start && x < end && y < start) ||
          (y >= start && y < end && x < end)) {
        filter[y*width + x] = 1.0;
      } else {
        filter[y*width + x] = 0.0;
      }
    }
  }
}

void do_filter() {
  //TODO: SIMDify this? It might autovectorize
  for (int i = 0; i < width*height; i++)
    dct_filtered_buf[i] = dct_buf[i] * filter[i];
}

void render_dct() {
  fftw_execute(idct_plan);

  for (int i = 0; i < width*height; i++) {
    double y_val = idct_buf[i]/width/height/4;
    if (y_val > 255.0)
      y_val = 255.0;
    if (y_val < 0.0)
      y_val = 0.0;
    buf[i*4] = (uint8_t)y_val;
    buf[i*4+1] = (uint8_t)y_val;
    buf[i*4+2] = (uint8_t)y_val;
  }
}

void paint_loop() {
  auto last_buf_swap = std::chrono::high_resolution_clock::now();
  int kRefreshPeriod = 33000;
  uint64_t frame_count = 0;
  int bandpass_start = 0;
  int bandpass_end = 0;
  int bandpass_dir = 5;
  std::unordered_map<int, uint8_t*> cache;
  while(1) {
    bandpass_end += bandpass_dir;
    printf("Bandpass end: %d\n", bandpass_end);
    if (bandpass_end >= width || bandpass_end < -1*bandpass_dir)
      bandpass_dir *= -1;
    if (!cache.count(bandpass_end)) {
      create_bandpass(bandpass_start, bandpass_end);
      do_filter();
      render_dct();
      display->swap_buf(buf);
      uint8_t* cache_entry = (uint8_t*)malloc(width*height*4);
      memcpy(cache_entry, buf, width*height*4);
      cache[bandpass_end] = cache_entry;
    } else {
      display->swap_buf(cache[bandpass_end]);
    }

    frame_count++;

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

  setup();

  QApplication app(argc, argv);

  display = new QtDisplay(width, height);

  paint_thread = new std::thread(paint_loop);

  return app.exec();
}
