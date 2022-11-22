#include <QApplication>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <png.h>
#include <thread>
#include <vector>

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

class Bolt {
private:
  std::vector<Coord> trace;
  std::vector<Coord> leads;
  static const uint32_t trace_color = 0xFF886666;
  uint32_t flash_color = 0xFFFFFFFF;
  const std::vector<uint32_t> walk_pdf = {10, 10, 10, 1};
  const std::vector<uint32_t> trace_split_pdf = {1000, 1};
  std::unique_ptr<MarkovSampler> walk_sampler = std::make_unique<MarkovSampler>(walk_pdf);
  std::unique_ptr<MarkovSampler> trace_split_sampler = std::make_unique<MarkovSampler>(trace_split_pdf);
  bool is_flashing = false;
  static const int flash_decay = 10;

public:
  Bolt(Coord seed);
  Bolt(Bolt&& bolt);

  bool is_done = false;
  void process();
  void render();
};

Bolt::Bolt(Coord seed) {
  leads.push_back(seed);
}

Bolt::Bolt(Bolt&& bolt) {
  trace = std::move(bolt.trace);
  leads = std::move(bolt.leads);
  walk_sampler = std::move(bolt.walk_sampler);
  trace_split_sampler = std::move(bolt.trace_split_sampler);
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

  std::vector<Coord> new_leads;
  for (Coord& lead : leads) {
    trace.emplace_back(lead);

    switch (walk_sampler->sample()) {
      case 0:
        lead.y++;
        break;
      case 1:
        lead.x--;
        break;
      case 2:
        lead.x++;
        break;
      default:
        lead.y--;
        break;
    }

    if (lead.y < 0)
      lead.y = 0;
    if (lead.x < 0)
      lead.x = 0;
    if (lead.x >= width)
      lead.x = width-1;
    if (lead.y >= height)
      lead.y = height-1;

    if (trace_split_sampler->sample())
      new_leads.emplace_back(lead);

    if (lead.y == height-1)
      is_flashing = true;
  }

  leads.insert(leads.end(), new_leads.begin(), new_leads.end());
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

  for (Coord& lead : leads) {
    color_buf[lead.y * width + lead.x] = flash_color;
  }
}


void paint_loop() {
  auto last_buf_swap = std::chrono::high_resolution_clock::now();
  int kRefreshPeriod = 33000;
  std::vector<uint32_t> new_bolt_pdf = {250, 1};
  MarkovSampler new_bolt_sampler(new_bolt_pdf);
  std::vector<Bolt> bolts;
  const int flash_decay = 1;
  while(1) {
    if (new_bolt_sampler.sample()) {
      Coord seed_coord;
      printf("New bolt!\n");
      seed_coord.y = 0;
      seed_coord.x = (rand() % (width - 2)) + 1;
      bolts.emplace_back(std::move(Bolt(seed_coord)));
    }

    std::vector<Bolt> next_cycle_bolts;
    for (Bolt& bolt : bolts) {
      for (int i = 0; i < 10; i++)
        bolt.process();
      bolt.render();
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
