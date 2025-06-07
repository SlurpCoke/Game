#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "asset_cache.h"
#include "list.h"
#include "sdl_wrapper.h"

static list_t *ASSET_CACHE;

const size_t FONT_SIZE = 18;
const size_t INITIAL_CAPACITY = 5;

typedef struct {
  asset_type_t type;
  char *filepath;
  void *obj;
} entry_t;

static void asset_cache_free_entry(entry_t *entry) {
  if (entry == NULL) {
    return;
  }

  if (entry->obj != NULL) {
    switch (entry->type) {
    case ASSET_IMAGE:
      SDL_DestroyTexture((SDL_Texture *)entry->obj);
      break;
    case ASSET_TEXT:
      TTF_CloseFont((TTF_Font *)entry->obj);
      break;
    }
  }

  free(entry->filepath);
  free(entry);
}

void asset_cache_init() {
  ASSET_CACHE =
      list_init(INITIAL_CAPACITY, (free_func_t)asset_cache_free_entry);
}

void asset_cache_destroy() { list_free(ASSET_CACHE); }

void *asset_cache_obj_get_or_create(asset_type_t ty, const char *filepath) {
  for (size_t i = 0; i < list_size(ASSET_CACHE); i++) {
    entry_t *entry = (entry_t *)list_get(ASSET_CACHE, i);
    if (strcmp(entry->filepath, filepath) == 0) {
      assert(entry->type == ty && "Asset found in cache has a mismatched type");
      return entry->obj;
    }
  }

  entry_t *new_entry = (entry_t *)malloc(sizeof(entry_t));

  new_entry->filepath = strdup(filepath);
  new_entry->type = ty;

  switch (ty) {
  case ASSET_IMAGE:
    new_entry->obj = sdl_get_image_texture(filepath);
    break;
  case ASSET_TEXT:
    new_entry->obj = TTF_OpenFont(filepath, FONT_SIZE);
    break;
  }

  list_add(ASSET_CACHE, new_entry);
  return new_entry->obj;
}