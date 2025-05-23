#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "asset.h"
#include "asset_cache.h"
#include "color.h"
#include "sdl_wrapper.h"
#include "body.h"
#include "list.h"

static list_t *ASSET_LIST = NULL;
const size_t INIT_CAPACITY = 5;

typedef struct asset {
  asset_type_t type;
  SDL_Rect bounding_box;
} asset_t;

typedef struct text_asset {
  asset_t base;
  TTF_Font *font;
  const char *text;
  color_t color;
} text_asset_t;

typedef struct image_asset {
  asset_t base;
  SDL_Texture *texture;
  body_t *body;
} image_asset_t;

/**
 * Allocates memory for an asset with the given parameters.
 *
 * @param ty the type of the asset
 * @param bounding_box the bounding box containing the location and dimensions
 * of the asset when it is rendered
 * @return a pointer to the newly allocated asset
 */
static asset_t *asset_init(asset_type_t ty, SDL_Rect bounding_box) {
  // This is a fancy way of malloc'ing space for an `image_asset_t` if `ty` is
  // ASSET_IMAGE, and `text_asset_t` otherwise.
  if (ASSET_LIST == NULL) {
    ASSET_LIST = list_init(INIT_CAPACITY, (free_func_t)asset_destroy);
  }
  asset_t *new =
      malloc(ty == ASSET_IMAGE ? sizeof(image_asset_t) : sizeof(text_asset_t));
  assert(new);
  new->type = ty;
  new->bounding_box = bounding_box;
  return new;
}

// Helper function to convert color_t to SDL_Color
static SDL_Color color_t_to_sdl_color(color_t c) {
  SDL_Color sdl_c;
  double r = c.red * 255.0;
  double g = c.green * 255.0;
  double b = c.blue * 255.0;

  sdl_c.r = (Uint8)(r < 0 ? 0 : (r > 255 ? 255 : r));
  sdl_c.g = (Uint8)(g < 0 ? 0 : (g > 255 ? 255 : g));
  sdl_c.b = (Uint8)(b < 0 ? 0 : (b > 255 ? 255 : b));
  sdl_c.a = 255;

  return sdl_c;
}

void asset_make_image_with_body(const char *filepath, body_t *body) {
  SDL_Rect arbitrary_rect = {0, 0, 0, 0};
  image_asset_t *new_image = (image_asset_t *)asset_init(ASSET_IMAGE, arbitrary_rect);

  new_image->texture = (SDL_Texture *)asset_cache_obj_get_or_create(ASSET_IMAGE, filepath);
  new_image->body = body;

  list_add(ASSET_LIST, new_image);
}

void asset_make_image(const char *filepath, SDL_Rect bounding_box) {
  image_asset_t *new_image =
      (image_asset_t *)asset_init(ASSET_IMAGE, bounding_box);

  new_image->texture =
      (SDL_Texture *)asset_cache_obj_get_or_create(ASSET_IMAGE, filepath);
  new_image->body = NULL;

  list_add(ASSET_LIST, new_image);
}

void asset_make_text(const char *filepath, SDL_Rect bounding_box, const char *text, color_t color) {
  text_asset_t *new_text = (text_asset_t *)asset_init(ASSET_TEXT, bounding_box);

  new_text->font =
      (TTF_Font *)asset_cache_obj_get_or_create(ASSET_TEXT, filepath);

  new_text->text = text;
  new_text->color = color;

  list_add(ASSET_LIST, new_text);
}

void asset_reset_asset_list() {
  if (ASSET_LIST != NULL) {
    list_free(ASSET_LIST);
  }
  ASSET_LIST = list_init(INIT_CAPACITY, (free_func_t)asset_destroy);
}

list_t *asset_get_asset_list() { return ASSET_LIST; }

void asset_remove_body(body_t *body) {
  if (ASSET_LIST == NULL || body == NULL) {
    return;
  }

  size_t i = 0;
  while (i < list_size(ASSET_LIST)) {
    asset_t *current_asset_base = (asset_t *)list_get(ASSET_LIST, i);
    bool removed_current = false;

    if (current_asset_base->type == ASSET_IMAGE) {
      image_asset_t *img_asset = (image_asset_t *)current_asset_base;
      if (img_asset->body == body) {
        void *removed_asset_ptr = list_remove(ASSET_LIST, i);
        asset_destroy(removed_asset_ptr);
        removed_current = true;
      }
    }

    if (!removed_current) {
      i++;
    }
  }
}

void asset_render(asset_t *asset) {
  if (asset == NULL) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Attempted to render a null asset");
    return;
  }

  switch (asset->type) {
  case ASSET_IMAGE: {
    image_asset_t *img_asset = (image_asset_t *)asset;
    if (img_asset->texture) {
      SDL_Rect render_rect = img_asset->base.bounding_box;

      if (img_asset->body != NULL) {
        render_rect = sdl_get_body_bounding_box(img_asset->body);
      }

      sdl_render_image(img_asset->texture, &render_rect);
    } else {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "Attempted to render an image asset with null texture");
    }
    break;
  }
  case ASSET_TEXT: {
    text_asset_t *txt_asset = (text_asset_t *)asset;
    if (txt_asset->font && txt_asset->text) {
      SDL_Color sdl_text_color = color_t_to_sdl_color(txt_asset->color);
      sdl_render_text(txt_asset->font, txt_asset->text, asset->bounding_box,
                      sdl_text_color);
    } else {
      if (!txt_asset->font)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Attempted to render a text asset with a null font");
      if (!txt_asset->text)
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Attempted to render a text asset with a null text");
    }
    break;
  }
  default:
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "unknown asset type in asset_render: %d", asset->type);
    assert(false && "Unknown asset type in asset_render");
    break;
  }
}

void asset_destroy(asset_t *asset) { free(asset); }