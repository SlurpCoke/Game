#ifndef __ASSET_H__
#define __ASSET_H__

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <color.h>
#include <sdl_wrapper.h>
#include <stddef.h>

#include "body.h"

typedef enum { ASSET_IMAGE, ASSET_TEXT } asset_type_t;

typedef struct asset asset_t;

/**
 * Allocates memory for an image asset with the given parameters and adds it 
 * to the internal asset list.
 *
 * @param filepath the filepath to the image file
 * @param bounding_box the bounding box containing the location and dimensions
 * of the text when it is rendered
 */
void *asset_make_image(const char *filepath, SDL_Rect bounding_box);

/**
 * Allocates memory for an image asset with an attached body and adds it 
 * to the internal asset list. When the asset is rendered, the image will be 
 * rendered on top of the body.
 *
 * @param filepath the filepath to the image file
 * @param body the body to render the image on top of
 */
void *asset_make_image_with_body(const char *filepath, body_t *body);

/**
 * Allocates memory for a text asset with the given parameters and adds it 
 * to the internal asset list.
 *
 * @param filepath the filepath to the .ttf file
 * @param bounding_box the bounding box containing the location and dimensions
 * of the text when it is rendered
 * @param text the text to render
 * @param color the color of the text
 */
void *asset_make_text(const char *filepath, SDL_Rect bounding_box,
                         const char *text, color_t color);

/**
 * Resets the internal asset list by freeing all assets and creating a new empty list.
 * This is useful when transitioning between scenes or levels.
 */
void asset_reset_asset_list();

/**
 * Returns the internal list of all assets that have been created.
 * 
 * @return a pointer to the list containing all assets
 */
list_t *asset_get_asset_list();

/**
 * Removes and destroys all image assets associated with the given body.
 * This is typically called when a body is destroyed to clean up its visual representation.
 * 
 * @param body the body whose associated assets should be removed
 */
void asset_remove_body(body_t *body);

/**
 * Renders the asset to the screen.
 * @param asset the asset to render
 */
void asset_render(asset_t *asset);

/**
 * Frees the memory allocated for the asset.
 * @param asset the asset to free
 */
void asset_destroy(asset_t *asset);

#endif // #ifndef __ASSET_H__
