#include <vector>
#include <stdint.h>

#ifndef MARKOV_H
#define MARKOV_H

class MarkovSampler {
private:
  std::vector<uint32_t> pdf;
  uint32_t sum;

public:
  MarkovSampler(const std::vector<uint32_t> pdf);
  int sample() const;
};

#endif
