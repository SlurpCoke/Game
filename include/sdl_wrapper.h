#ifndef __SDL_WRAPPER_H__
#define __SDL_WRAPPER_H__

#include <SDL2/SDL.h>
#include "color.h"
#include "list.h"
#include "scene.h"
#include "body.h"
#include "state.h"
#include "vector.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

// Values passed to a key handler when the given arrow key is pressed
typedef enum {
  LEFT_ARROW = 1,
  UP_ARROW = 2,
  RIGHT_ARROW = 3,
  DOWN_ARROW = 4,
  SPACE_BAR = 5,
} arrow_key_t;

/**
 * The possible types of key events.
 * Enum types in C are much more primitive than in Java; this is equivalent to:
 * typedef unsigned int KeyEventType;
 * #define KEY_PRESSED 0
 * #define KEY_RELEASED 1
 */
typedef enum { KEY_PRESSED, KEY_RELEASED } key_event_type_t;

/**
 * A keypress handler.
 * When a key is pressed or released, the handler is passed its char value.
 * Most keys are passed as their char value, e.g. 'a', '1', or '\r'.
 * Arrow keys have the special values listed above.
 *
 * @param key a character indicating which key was pressed
 * @param type the type of key event (KEY_PRESSED or KEY_RELEASED)
 * @param held_time if a press event, the time the key has been held in seconds
 */
typedef void (*key_handler_t)(char key, key_event_type_t type, double held_time,
                              void *state);

/**
 * Initializes the SDL window and renderer.
 * Must be called once before any of the other SDL functions.
 *
 * @param min the x and y coordinates of the bottom left of the scene
 * @param max the x and y coordinates of the top right of the scene
 */
void sdl_init(vector_t min, vector_t max);

/**
 * Processes all SDL events and returns whether the window has been closed.
 * This function must be called in order to handle keypresses.
 *
 * @return true if the window was closed, false otherwise
 */
bool sdl_is_done(state_t *state);

/**
 * Clears the screen. Should be called before drawing bodies in each frame.
 */
void sdl_clear(void);

/**
 * Draws a body using the color of the body.
 *
 * @param body the body struct to draw
 */
void sdl_draw_body(body_t *body);

/**
 * Loads an image from a file and returns it as an SDL texture.
 *
 * @param image_path the file path to the image
 * @return a pointer to the loaded texture
 */
SDL_Texture *sdl_get_image_texture(const char *image_path);

/**
 * Creates an SDL_Rect with the specified dimensions.
 *
 * @param x the x-coordinate of the rectangle
 * @param y the y-coordinate of the rectangle
 * @param w the width of the rectangle
 * @param h the height of the rectangle
 * @return a pointer to the created rectangle
 */
SDL_Rect *sdl_get_rect(double x, double y, double w, double h);

/**
 * Calculates smallest SDL_Rect enclosing given body
 * 
 * @param body a pointer to body_t
 * @return an SDL_Rect representing the bounding box of the body
 */
SDL_Rect sdl_get_body_bounding_box(body_t *body);

/**
 * Renders an image to the screen using the specified texture and rectangle.
 *
 * @param image_texture the texture to render
 * @param rect the rectangle defining the position and size of the rendered
 * image
 */
void sdl_render_image(SDL_Texture *image_texture, SDL_Rect *rect);

/**
 * Renders text to the screen using font, text, destination, and color
 * Text isi rendered onto a temporary surface, then texture, then copied to
 * renderer, stretched to fit destination rectangle
 *
 * @param font the TTF_Font to use
 * @param text the string to render
 * @param dest_rect the SDL_Rect defining the position/size for rendered text
 * @param color the SDL_Color to use
 */
void sdl_render_text(TTF_Font *font, const char *text, SDL_Rect dest_rect,
                     SDL_Color color);

/**
 * Displays the rendered frame on the SDL window.
 * Must be called after drawing the bodies in order to show them.
 */
void sdl_show(void);

/**
 * Draws all bodies in a scene.
 * This internally calls sdl_clear(), sdl_draw_body(), and sdl_show(),
 * so those functions should not be called directly.
 *
 * @param scene the scene to draw
 */
void sdl_render_scene(scene_t *scene);

/**
 * Registers a function to be called every time a key is pressed.
 * Overwrites any existing handler.
 *
 * Example:
 * ```
 * void on_key(char key, key_event_type_t type, double held_time) {
 *     if (type == KEY_PRESSED) {
 *         switch (key) {
 *             case 'a':
 *                 printf("A pressed\n");
 *                 break;
 *             case UP_ARROW:
 *                 printf("UP pressed\n");
 *                 break;
 *         }
 *     }
 * }
 * int main(void) {
 *     sdl_on_key(on_key);
 *     while (!sdl_is_done());
 * }
 * ```
 *
 * @param handler the function to call with each key press
 */
void sdl_on_key(key_handler_t handler);

/**
 * Gets the amount of time that has passed since the last time
 * this function was called, in seconds.
 *
 * @return the number of seconds that have elapsed
 */
double time_since_last_tick(void);

#endif // #ifndef __SDL_WRAPPER_H__