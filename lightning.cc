#include <QApplication>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <png.h>
#include <thread>
#include <vector>
#include <math.h>

#include "qt_display.h"
#include "markov.h"

int width = 1000;
int height = 1000;
uint8_t* buf;
QtDisplay* display;
std::thread* paint_thread;

struct Coord {
  int x;
  int y;
};

class Lead {
public:
  Coord coord;
  double heading_angle;
  std::vector<uint32_t> pdf;
  std::unique_ptr<MarkovSampler> walk_sampler;

  Lead(Coord coord, double heading_angle);
  Lead(Lead&& lead);
};

void vector_pdf_helper(int& dir1, int& dir2, int vector_component, int amplitude) {
  if (vector_component >= 0) {
    dir1 = amplitude/2 + vector_component/2;
    dir2 = amplitude - dir1;
  } else {
    dir2 = amplitude/2 - vector_component/2;
    dir1 = amplitude - dir2;
  }
}

Lead::Lead(Coord coord, double heading_angle) {
  this->coord = coord;
  this->heading_angle = heading_angle;

  int vec_x, vec_y, ortho_vec_x, ortho_vec_y;
  vec_x = (int)(sin(heading_angle) * 100.0);
  vec_y = (int)(cos(heading_angle) * 100.0);
  ortho_vec_x = -1*vec_y;
  ortho_vec_y = vec_x;

  int left, right, up, down;
  vector_pdf_helper(right, left, vec_x, 100);
  vector_pdf_helper(down, up, vec_y, 100);

  int left_noise, right_noise, up_noise, down_noise;
  vector_pdf_helper(right_noise, left_noise, ortho_vec_x, 100);
  vector_pdf_helper(down_noise, up_noise, ortho_vec_y, 100);
  left += left_noise;
  right += right_noise;
  up += up_noise;
  down += down_noise;
  ortho_vec_x *= -1;
  ortho_vec_y *= -1;
  vector_pdf_helper(right_noise, left_noise, ortho_vec_x, 100);
  vector_pdf_helper(down_noise, up_noise, ortho_vec_y, 100);
  left += left_noise;
  right += right_noise;
  up += up_noise;
  down += down_noise;

//  printf("angle: %f\n", heading_angle);
//  printf("vec_x: %d, vec_y: %d\n", vec_x, vec_y);
//  printf("down: %d, left: %d, right: %d, up: %d\n", down, left, right, up);

  pdf.push_back(down);
  pdf.push_back(left);
  pdf.push_back(right);
  pdf.push_back(up);
  walk_sampler = std::make_unique<MarkovSampler>(pdf);
}

Lead::Lead(Lead&& lead) {
  coord = lead.coord;
  heading_angle = lead.heading_angle;
  pdf = std::move(lead.pdf);
  walk_sampler = std::move(lead.walk_sampler);
}

class Bolt {
private:
  std::vector<Coord> trace;
  std::vector<Lead> leads;
  static const uint32_t trace_color = 0xFF444488;
  uint32_t flash_color = 0xFFFFFFFF;
  const std::vector<uint32_t> trace_split_pdf = {1000, 1};
  std::unique_ptr<MarkovSampler> trace_split_sampler = std::make_unique<MarkovSampler>(trace_split_pdf);
  const std::vector<uint32_t> split_dir_pdf = {1, 1};
  std::unique_ptr<MarkovSampler> split_dir_sampler = std::make_unique<MarkovSampler>(split_dir_pdf);
  bool is_flashing = false;
  static const int flash_decay = 1;

public:
  Bolt(Coord seed);
  Bolt(Bolt&& bolt);

  bool is_done = false;
  void process();
  void render();
};

Bolt::Bolt(Coord seed) {
  Lead primary(seed, 0.0);
  leads.emplace_back(std::move(primary));
}

Bolt::Bolt(Bolt&& bolt) {
  trace = std::move(bolt.trace);
  leads = std::move(bolt.leads);
  trace_split_sampler = std::move(bolt.trace_split_sampler);
  split_dir_sampler = std::move(split_dir_sampler);
  flash_color = bolt.flash_color;
  is_done = bolt.is_done;
  is_flashing = bolt.is_flashing;
}

void Bolt::process() {
  if (is_done)
    return;

  if (is_flashing) {
    if (flash_color == 0xFF000000 || (flash_color & 0xFF) < flash_decay) {
      flash_color = 0xFF000000;
      is_done = true;
    } else {
      flash_color -= flash_decay;
      flash_color -= (flash_decay << 8);
      flash_color -= (flash_decay << 16);
    }
    return;
  }

  std::vector<Lead> new_leads;
  for (Lead& lead : leads) {
    trace.emplace_back(lead.coord);

    switch (lead.walk_sampler->sample()) {
      case 0:
        lead.coord.y++;
        break;
      case 1:
        lead.coord.x--;
        break;
      case 2:
        lead.coord.x++;
        break;
      default:
        lead.coord.y--;
        break;
    }

    if (lead.coord.y < 0) {
      lead.coord.y = 0;
      is_flashing = true;
    } else if (lead.coord.x < 0) {
      lead.coord.x = 0;
      is_flashing = true;
    } else if (lead.coord.x >= width) { 
      lead.coord.x = width-1;
      is_flashing = true;
    } else if (lead.coord.y >= height) {
      lead.coord.y = height-1;
      is_flashing = true;
    } else if (trace_split_sampler->sample()) {
      int dir = split_dir_sampler->sample();
      //printf("dir: %d\n", dir);
      float new_heading_angle = dir ? lead.heading_angle + M_PI_4 : lead.heading_angle - M_PI_4;
      Lead new_lead(lead.coord, new_heading_angle);
      new_leads.emplace_back(std::move(new_lead));
    }
  }

  for (Lead& lead : new_leads)
    leads.emplace_back(std::move(lead));
}

void Bolt::render() {
  uint32_t* color_buf = (uint32_t*)buf;

  for (Coord& trace_point : trace) {
    if (is_flashing) {
      color_buf[trace_point.y * width + trace_point.x] = flash_color;
    } else {
      color_buf[trace_point.y * width + trace_point.x] = trace_color;
    }
  }

  for (Lead& lead : leads) {
    color_buf[lead.coord.y * width + lead.coord.x] = flash_color;
  }
}

void process_bolt(std::vector<Bolt>* bolts, int idx) {
  for (int i = 0; i < 10; i++)
    (*bolts)[idx].process();

  (*bolts)[idx].render();
}

void paint_loop() {
  auto last_buf_swap = std::chrono::high_resolution_clock::now();
  int kRefreshPeriod = 33000;
  std::vector<uint32_t> new_bolt_pdf = {150, 1};
  MarkovSampler new_bolt_sampler(new_bolt_pdf);
  std::vector<Bolt> bolts;
  const int flash_decay = 1;
  bool first_frame = true;
  while(1) {
    if (first_frame || new_bolt_sampler.sample()) {
      Coord seed_coord;
      //printf("New bolt!\n");
      seed_coord.y = 0;
      seed_coord.x = (rand() % (width - 2)) + 1;
      bolts.emplace_back(std::move(Bolt(seed_coord)));
      first_frame = false;
    }

    std::vector<Bolt> next_cycle_bolts;
    std::vector<std::shared_ptr<std::thread>> bolt_threads;
    for (int i = 0; i < bolts.size(); i++)
      bolt_threads.emplace_back(std::make_shared<std::thread>(process_bolt, &bolts, i));

    for (auto thread : bolt_threads)
      thread->join();

    for (Bolt& bolt : bolts) {
      if (!bolt.is_done)
        next_cycle_bolts.emplace_back(std::move(bolt));
    }
    bolts = std::move(next_cycle_bolts);

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

  QApplication app(argc, argv);

  display = new QtDisplay(width, height);

  paint_thread = new std::thread(paint_loop);

  return app.exec();
}
