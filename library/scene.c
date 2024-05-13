#include "scene.h"

struct scene {
  size_t num_bodies;
  list_t *bodies;
  list_t *force_creators;
};