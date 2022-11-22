#include "markov.h"

#include <stdlib.h>

MarkovSampler::MarkovSampler(const std::vector<uint32_t> pdf) {
  sum = 0;
  for (uint32_t density : pdf) {
    sum += density;
  }

  this->pdf = pdf;
}

int MarkovSampler::sample() const {
  uint32_t rand_val = rand() % sum;
  uint32_t cdf = 0;

  for (int i = 0; i < pdf.size(); i++) {
    cdf += pdf[i];
    if (cdf > rand_val)
      return i;
  }

  return -1;
}
