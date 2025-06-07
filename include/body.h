#ifndef __BODY_H__
#define __BODY_H__

#include <stdbool.h>

#include "color.h"
#include "list.h"
#include "vector.h"

/**
 * A rigid body constrained to the plane.
 * Implemented as a polygon with uniform density.
 */
typedef struct body body_t;

/**
 * Initializes a body without any info.
 * Acts like body_init_with_info() where info and info_freer are NULL.
 */

body_t *body_init(list_t *shape, double mass, color_t color);

/**
 * Allocates memory for a body with the given parameters.
 * Gains ownership of the shape list.
 * The body is initially at rest.
 * Asserts that the required memory is allocated.
 *
 * @param shape a list of vectors describing the initial shape of the body
 * @param mass the mass of the body (if INFINITY, stops the body from moving)
 * @param color the color of the body, used to draw it on the screen
 * @param info additional information to associate with the body,
 *   e.g. its type if the scene has multiple types of bodies
 * @param info_freer if non-NULL, a function call on the info to free it
 * @return a pointer to the newly allocated body
 */
body_t *body_init_with_info(list_t *shape, double mass, color_t color,
                            void *info, free_func_t info_freer);

/**
 * Gets the current shape of a body.
 * Returns a newly allocated vector list, which must be list_free()d.
 *
 * @param body the pointer to the body
 * @return a list of vectors
 */
list_t *body_get_shape(body_t *body);

/**
 * Return the info associated with a body.
 *
 * @param body the pointer to the body
 * @return a pointer to the info associated with a body
 */
void *body_get_info(body_t *body);

/**
 * Gets the current center of mass of a body.
 *
 * @param body the pointer to the body
 * @return the body's center of mass
 */
vector_t body_get_centroid(body_t *body);

/**
 * Translates a body to a new position.
 * The position is specified by the position of the body's center of mass.
 *
 * @param body the pointer to the body
 * @param x the body's new centroid
 */
void body_set_centroid(body_t *body, vector_t x);

/**
 * Gets the current velocity of a body.
 *
 * @param body the pointer to the body
 * @return the body's velocity vector
 */
vector_t body_get_velocity(body_t *body);

/**
 * Changes a body's velocity (the time-derivative of its position).
 *
 * @param body the pointer to the body
 * @param v the body's new velocity
 */
void body_set_velocity(body_t *body, vector_t v);

/**
 * Returns a body's area.
 * See https://en.wikipedia.org/wiki/Shoelace_formula#Statement.
 *
 * @param body the pointer to the body
 * @return the area of the body
 */
double body_area(body_t *body);

/**
 * Returns a body's color.
 *
 * @param body the pointer to the body
 * @return the color_t struct representing the color
 */
color_t body_get_color(body_t *body);

/**
 * Sets the display color of a body.
 *
 * @param body the pointer to the body
 * @param color the body's color
 */
void body_set_color(body_t *body, color_t color);

/**
 * Gets the rotation angle of a body.
 *
 * @param body the pointer to the body
 * @return the body's rotation angle in radians
 */
double body_get_rotation(body_t *body);

/**
 * Changes a body's orientation in the plane.
 * The body is rotated about its center of mass.
 * Note that the angle is *absolute*, not relative to the current orientation.
 *
 * @param body the pointer to the body
 * @param angle the body's new angle in radians. Positive is counterclockwise.
 */
void body_set_rotation(body_t *body, double angle);

/**
 * Updates the body after a given time interval has elapsed.
 * Sets acceleration and velocity according to the forces and impulses
 * applied to the body during the tick.
 * The body is translated at the *average* of the velocities before
 * and after the tick.
 * Resets the forces and impulses accumulated on the body.
 *
 * @param body the body to tick
 * @param dt the number of seconds elapsed since the last tick
 */
void body_tick(body_t *body, double dt);

/**
 * Returns the mass of a body.
 *
 * @param body the pointer to the body
 * @return the body's mass
 */
double body_get_mass(body_t *body);

/**
 * Applies a force to a body over the current tick.
 * If multiple forces are applied in the same tick, they are added.
 * Does not change the body's position or velocity; see body_tick().
 *
 * @param body the pointer to the body
 * @param force the force vector to apply
 */
void body_add_force(body_t *body, vector_t force);

/**
 * Applies an impulse to a body.
 * An impulse causes an instantaneous change in velocity,
 * which is useful for modeling collisions.
 * If multiple impulses are applied in the same tick, they are added.
 * Does not change the body's position or velocity; see body_tick().
 *
 * @param body the pointer to the body
 * @param impulse the impulse vector to apply
 */
void body_add_impulse(body_t *body, vector_t impulse);

/**
 * Clear the forces and impulses on the body.
 *
 * @param body the pointer to the body
 */
void body_reset(body_t *body);

/**
 * Marks a body for removal--future calls to body_is_removed() will return
 * `true`. Does not free the body.
 * If the body is already marked for removal,
 * does nothing.
 *
 * @param body the body to mark for removal
 */
void body_remove(body_t *body);

/**
 * Returns whether a body has been marked for removal.
 * This function returns false until body_remove() is called on the body,
 * and returns true afterwards.
 *
 * @param body the pointer to the body
 * @return whether body_remove() has been called on the body
 */
bool body_is_removed(body_t *body);

/**
 * Frees memory allocated for a body.
 *
 * @param body the pointer to the body
 */
void body_free(body_t *body);

#endif // #ifndef __BODY_H__




#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "asset.h"
#include "asset_cache.h"
#include "body.h"
#include "collision.h"
#include "color.h"
#include "forces.h"
#include "list.h"
#include "scene.h"
#include "sdl_wrapper.h"
#include "state.h"
#include "vector.h"

const vector_t MIN_SCREEN_COORDS = {0, 0};
const vector_t MAX_SCREEN_COORDS = {1000, 500};

// Colors
const color_t PLAYER_COLOR = (color_t){0.0, 0.8, 0.0};
const color_t ENEMY_COLOR_1 = (color_t){0.8, 0.0, 0.0};
const color_t ENEMY_COLOR_2 = (color_t){0.9, 0.4, 0.0};
const color_t BULLET_COLOR = (color_t){0.9, 0.9, 0.2};
const color_t WATER_COLOR = (color_t){0.2, 0.2, 0.8};
const color_t PLATFORM_COLOR = (color_t){0.5, 0.5, 0.5};
const color_t HP_BAR_BG_COLOR = (color_t){0.4, 0.0, 0.0};
const color_t HP_BAR_FG_COLOR = (color_t){0.0, 1.0, 0.0};
const color_t WATER_DEATH_COLOR = (color_t){0.1, 0.1, 0.1};

// Sizes and Positions
const double PLATFORM_WIDTH = 150.0;
const double PLATFORM_HEIGHT = 20.0;
const double CHARACTER_SIZE = 30.0;
const double BULLET_WIDTH = 20.0;
const double BULLET_HEIGHT = 10.0;
const double WATER_Y_CENTER = 10.0;
const double WATER_HEIGHT = 20.0;
const double HP_BAR_WIDTH = 40.0;
const double HP_BAR_HEIGHT = 5.0;
const double HP_BAR_Y_OFFSET = 25.0;

const double PLATFORM_Y =
    WATER_Y_CENTER + WATER_HEIGHT / 2.0 + 20.0 + PLATFORM_HEIGHT / 2.0;
const vector_t PLATFORM_L_POS = {150, PLATFORM_Y};
const vector_t PLATFORM_R_POS = {MAX_SCREEN_COORDS.x - 150, PLATFORM_Y};
const vector_t PLAYER_START_POS = {PLATFORM_L_POS.x - 50,
                                   PLATFORM_L_POS.y + PLATFORM_HEIGHT / 2.0 +
                                       CHARACTER_SIZE / 2.0 + 0.1};
const vector_t ENEMY1_START_POS = {PLATFORM_R_POS.x - CHARACTER_SIZE / 1.5,
                                   PLATFORM_R_POS.y + PLATFORM_HEIGHT / 2.0 +
                                       CHARACTER_SIZE / 2.0 + 0.1};
const vector_t ENEMY2_START_POS = {PLATFORM_R_POS.x + CHARACTER_SIZE / 1.5,
                                   PLATFORM_R_POS.y + PLATFORM_HEIGHT / 2.0 +
                                       CHARACTER_SIZE / 2.0 + 0.1};

// Physics & Game Constants
const double BULLET_VELOCITY_X = 250.0;
const double GRAVITY_ACCELERATION = 250.0;
const double KNOCKBACK_BASE_VELOCITY_X = 40.0;
const double KNOCKBACK_BASE_VELOCITY_Y = 100.0;
const double MAX_HEALTH = 100.0;

const char *BACKGROUND_PATH = "assets/frogger-background.png";

typedef enum {
  TYPE_PLAYER,
  TYPE_ENEMY,
  TYPE_BULLET_PLAYER,
  TYPE_BULLET_ENEMY1,
  TYPE_BULLET_ENEMY2,
  TYPE_WATER,
  TYPE_PLATFORM,
  TYPE_HP_BAR
} body_type_t;

typedef struct {
  body_type_t type;
  bool affected_by_gravity;
  bool is_knocked_back;
  int id;
  double current_hp;
  double max_hp;
} character_info_t;

typedef enum {
  WAITING_FOR_PLAYER_SHOT,
  PLAYER_SHOT_ACTIVE,
  ENEMY1_FIRING,
  ENEMY1_SHOT_ACTIVE,
  ENEMY2_FIRING,
  ENEMY2_SHOT_ACTIVE,
  GAME_OVER_WATER
} game_status_t;

// This was corrupted in the last response. Here is the correct definition.
struct state {
  scene_t *scene;
  body_t *player;
  body_t *enemy1;
  body_t *enemy2;
  body_t *water_body;
  body_t *platform_l;
  body_t *platform_r;
  body_t *player_hp_bar_fg;
  body_t *player_hp_bar_bg;
  body_t *enemy1_hp_bar_fg;
  body_t *enemy1_hp_bar_bg;
  body_t *enemy2_hp_bar_fg;
  body_t *enemy2_hp_bar_bg;
  list_t *all_characters;
  game_status_t current_status;
};

// Forward declarations
void bullet_hit_target_handler(body_t *bullet, body_t *target, vector_t axis,
                               void *aux, double force_const);
void character_hit_water_handler(body_t *character, body_t *water,
                                 vector_t axis, void *aux, double force_const);
void apply_conditional_gravity(void *aux_state, list_t *all_characters);
void character_platform_contact_handler(body_t *character, body_t *platform,
                                        vector_t axis, void *aux,
                                        double force_const);
void draw_hp_bars(state_t *state); // *** FIX: ADDED FORWARD DECLARATION ***

body_t *make_generic_rectangle_body(vector_t center, double width,
                                    double height, color_t color,
                                    body_type_t type_tag, double mass) {
  list_t *shape = list_init(4, free);
  vector_t *v1 = malloc(sizeof(vector_t));
  *v1 = (vector_t){-width / 2, -height / 2};
  list_add(shape, v1);
  vector_t *v2 = malloc(sizeof(vector_t));
  *v2 = (vector_t){+width / 2, -height / 2};
  list_add(shape, v2);
  vector_t *v3 = malloc(sizeof(vector_t));
  *v3 = (vector_t){+width / 2, +height / 2};
  list_add(shape, v3);
  vector_t *v4 = malloc(sizeof(vector_t));
  *v4 = (vector_t){-width / 2, +height / 2};
  list_add(shape, v4);

  body_type_t *info = malloc(sizeof(body_type_t));
  *info = type_tag;

  body_t *rect_body = body_init_with_info(shape, mass, color, info, free);
  body_set_centroid(rect_body, center);
  return rect_body;
}

body_t *make_character_body(vector_t center, double size, color_t color,
                            body_type_t type, int id, double mass) {
  list_t *shape = list_init(4, free);
  vector_t *v1 = malloc(sizeof(vector_t));
  *v1 = (vector_t){-size / 2, -size / 2};
  list_add(shape, v1);
  vector_t *v2 = malloc(sizeof(vector_t));
  *v2 = (vector_t){+size / 2, -size / 2};
  list_add(shape, v2);
  vector_t *v3 = malloc(sizeof(vector_t));
  *v3 = (vector_t){+size / 2, +size / 2};
  list_add(shape, v3);
  vector_t *v4 = malloc(sizeof(vector_t));
  *v4 = (vector_t){-size / 2, +size / 2};
  list_add(shape, v4);

  character_info_t *info = malloc(sizeof(character_info_t));
  info->type = type;
  info->affected_by_gravity = false;
  info->is_knocked_back = false;
  info->id = id;
  info->max_hp = MAX_HEALTH;
  info->current_hp = MAX_HEALTH;

  body_t *char_body = body_init_with_info(shape, mass, color, info, free);
  body_set_centroid(char_body, center);
  return char_body;
}

body_t *make_projectile_body(vector_t center, double width, double height,
                             color_t color, body_type_t type, int shooter_id,
                             double mass) {
  list_t *shape = list_init(4, free);
  vector_t *v1 = malloc(sizeof(vector_t));
  *v1 = (vector_t){-width / 2, -height / 2};
  list_add(shape, v1);
  vector_t *v2 = malloc(sizeof(vector_t));
  *v2 = (vector_t){+width / 2, -height / 2};
  list_add(shape, v2);
  vector_t *v3 = malloc(sizeof(vector_t));
  *v3 = (vector_t){+width / 2, +height / 2};
  list_add(shape, v3);
  vector_t *v4 = malloc(sizeof(vector_t));
  *v4 = (vector_t){-width / 2, +height / 2};
  list_add(shape, v4);

  character_info_t *info = malloc(sizeof(character_info_t));
  info->type = type;
  info->affected_by_gravity = false;
  info->is_knocked_back = false;
  info->id = shooter_id;
  info->current_hp = 0;
  info->max_hp = 0;

  body_t *projectile = body_init_with_info(shape, mass, color, info, free);
  body_set_centroid(projectile, center);
  return projectile;
}

body_t *fire_bullet(state_t *current_state, body_t *shooter,
                    body_t *target_to_aim_at, body_type_t bullet_tag) {
  vector_t shooter_pos = body_get_centroid(shooter);
  character_info_t *shooter_info = (character_info_t *)body_get_info(shooter);
  vector_t fire_direction =
      vec_subtract(body_get_centroid(target_to_aim_at), shooter_pos);
  double dir_x = (fire_direction.x >= 0) ? 1.0 : -1.0;
  vector_t bullet_start_pos = vec_add(
      shooter_pos,
      (vector_t){dir_x * (CHARACTER_SIZE / 2.0 + BULLET_WIDTH / 2.0 + 5.0), 0});
  vector_t bullet_velocity = {dir_x * BULLET_VELOCITY_X, 0};

  body_t *bullet = make_projectile_body(
      bullet_start_pos, BULLET_WIDTH, BULLET_HEIGHT, BULLET_COLOR, bullet_tag,
      shooter_info ? shooter_info->id : -1, 1.0);
  body_set_velocity(bullet, bullet_velocity);
  scene_add_body(current_state->scene, bullet);

  if (bullet_tag == TYPE_BULLET_PLAYER) {
    create_collision(current_state->scene, bullet, current_state->enemy1,
                     bullet_hit_target_handler, current_state, 0.0, NULL);
    create_collision(current_state->scene, bullet, current_state->enemy2,
                     bullet_hit_target_handler, current_state, 0.0, NULL);
  } else if (bullet_tag == TYPE_BULLET_ENEMY1 ||
             bullet_tag == TYPE_BULLET_ENEMY2) {
    create_collision(current_state->scene, bullet, current_state->player,
                     bullet_hit_target_handler, current_state, 0.0, NULL);
  }
  return bullet;
}

void on_key_press(char key, key_event_type_t type, double held_time,
                  state_t *current_state) {
  if (type == KEY_PRESSED && current_state->current_status != GAME_OVER_WATER) {
    if (key == SPACE_BAR &&
        current_state->current_status == WAITING_FOR_PLAYER_SHOT) {
      printf("Player firing.\n");
      fire_bullet(current_state, current_state->player, current_state->enemy1,
                  TYPE_BULLET_PLAYER);
      current_state->current_status = PLAYER_SHOT_ACTIVE;
    }
  }
}

void bullet_hit_target_handler(body_t *bullet, body_t *target, vector_t axis,
                               void *aux, double force_const) {
  state_t *current_state = (state_t *)aux;
  if (current_state->current_status == GAME_OVER_WATER) {
    body_remove(bullet);
    return;
  }

  character_info_t *target_char_info =
      (character_info_t *)body_get_info(target);
  if (!target_char_info || target_char_info->current_hp <= 0 ||
      body_is_removed(target)) {
    body_remove(bullet);
    return;
  }

  target_char_info->current_hp -= target_char_info->max_hp / 2.0;
  if (target_char_info->current_hp < 0)
    target_char_info->current_hp = 0;
  printf("Character ID %d HP is now %.1f/%.1f\n", target_char_info->id,
         target_char_info->current_hp, target_char_info->max_hp);

  // The HP bar body is removed/recreated in the main loop for stability
  // Here we just set the flags and velocity for knockback

  target_char_info->affected_by_gravity = true;
  target_char_info->is_knocked_back = true;
  vector_t bullet_vel = body_get_velocity(bullet);
  double knock_dir_x = (bullet_vel.x > 0) ? 1.0 : -1.0;
  body_set_velocity(target, (vector_t){knock_dir_x * KNOCKBACK_BASE_VELOCITY_X,
                                       KNOCKBACK_BASE_VELOCITY_Y});

  // Change turn logic
  body_type_t bullet_type = ((character_info_t *)body_get_info(bullet))->type;
  if (bullet_type == TYPE_BULLET_PLAYER)
    current_state->current_status = ENEMY1_FIRING;
  else if (bullet_type == TYPE_BULLET_ENEMY1)
    current_state->current_status = ENEMY2_FIRING;
  else if (bullet_type == TYPE_BULLET_ENEMY2)
    current_state->current_status = WAITING_FOR_PLAYER_SHOT;

  body_remove(bullet);
}

void character_hit_water_handler(body_t *character, body_t *water,
                                 vector_t axis, void *aux, double force_const) {
  state_t *current_state = (state_t *)aux;
  character_info_t *char_info = (character_info_t *)body_get_info(character);

  if (!char_info || current_state->current_status == GAME_OVER_WATER)
    return;

  color_t current_color = body_get_color(character);
  if (fabs(current_color.red - WATER_DEATH_COLOR.red) > 0.01 ||
      fabs(current_color.green - WATER_DEATH_COLOR.green) > 0.01 ||
      fabs(current_color.blue - WATER_DEATH_COLOR.blue) > 0.01) {
    printf("Character ID %d touched water.\n", char_info->id);
    body_set_color(character, WATER_DEATH_COLOR);
    body_set_velocity(character, VEC_ZERO);
    char_info->affected_by_gravity = false;
    char_info->is_knocked_back = false;
    current_state->current_status = GAME_OVER_WATER;
    printf("Game Over! Character ID %d fell into the water.\n", char_info->id);
  }
}

void apply_conditional_gravity(void *aux_list, list_t *bodies_affected) {
  list_t *all_characters = (list_t *)aux_list;
  for (size_t i = 0; i < list_size(all_characters); i++) {
    body_t *body = (body_t *)list_get(all_characters, i);
    if (body_is_removed(body))
      continue;

    character_info_t *info = (character_info_t *)body_get_info(body);

    if (info && (info->type == TYPE_PLAYER || info->type == TYPE_ENEMY)) {
      if (info->affected_by_gravity && body_get_mass(body) != INFINITY) {
        vector_t gravity_force = {0,
                                  -body_get_mass(body) * GRAVITY_ACCELERATION};
        body_add_force(body, gravity_force);
      }
    }
  }
}

void character_platform_contact_handler(body_t *character, body_t *platform,
                                        vector_t axis, void *aux,
                                        double force_const) {
  state_t *current_state = (state_t *)aux;
  if (current_state->current_status == GAME_OVER_WATER)
    return;

  character_info_t *char_info = (character_info_t *)body_get_info(character);
  if (!char_info ||
      (char_info->type != TYPE_PLAYER && char_info->type != TYPE_ENEMY))
    return;

  vector_t char_pos = body_get_centroid(character);
  vector_t platform_pos = body_get_centroid(platform);
  double char_half_height = CHARACTER_SIZE / 2.0;
  double platform_half_height = PLATFORM_HEIGHT / 2.0;
  double char_bottom = char_pos.y - char_half_height;
  double platform_top = platform_pos.y + platform_half_height;

  if (axis.y > 0.7 && char_bottom <= platform_top + 2.0) {
    if (char_info->is_knocked_back) {
      printf("Character ID %d landed on platform after knockback.\n",
             char_info->id);
      body_set_velocity(character, VEC_ZERO);
      char_info->is_knocked_back = false;
      char_info->affected_by_gravity = false;
    }

    if (!char_info->affected_by_gravity) {
      vector_t current_vel = body_get_velocity(character);
      if (current_vel.y < 0)
        body_set_velocity(character, (vector_t){current_vel.x, 0.0});
      body_set_centroid(
          character,
          (vector_t){char_pos.x, platform_top + char_half_height + 0.01});
    }
  }
}

state_t *emscripten_init() {
  asset_cache_init();
  sdl_init(MIN_SCREEN_COORDS, MAX_SCREEN_COORDS);
  srand(time(NULL));

  state_t *current_state = malloc(sizeof(state_t));
  assert(current_state != NULL);

  current_state->scene = scene_init();
  current_state->current_status = WAITING_FOR_PLAYER_SHOT;
  current_state->all_characters = list_init(3, NULL);

  asset_make_image(BACKGROUND_PATH, (SDL_Rect){0, 0, (int)MAX_SCREEN_COORDS.x,
                                               (int)MAX_SCREEN_COORDS.y});

  current_state->water_body = make_generic_rectangle_body(
      (vector_t){MAX_SCREEN_COORDS.x / 2.0, WATER_Y_CENTER},
      MAX_SCREEN_COORDS.x, WATER_HEIGHT, WATER_COLOR, TYPE_WATER, INFINITY);
  scene_add_body(current_state->scene, current_state->water_body);
  current_state->platform_l = make_generic_rectangle_body(
      PLATFORM_L_POS, PLATFORM_WIDTH, PLATFORM_HEIGHT, PLATFORM_COLOR,
      TYPE_PLATFORM, INFINITY);
  scene_add_body(current_state->scene, current_state->platform_l);
  current_state->platform_r = make_generic_rectangle_body(
      PLATFORM_R_POS, PLATFORM_WIDTH, PLATFORM_HEIGHT, PLATFORM_COLOR,
      TYPE_PLATFORM, INFINITY);
  scene_add_body(current_state->scene, current_state->platform_r);

  current_state->player = make_character_body(
      PLAYER_START_POS, CHARACTER_SIZE, PLAYER_COLOR, TYPE_PLAYER, 0, 10.0);
  scene_add_body(current_state->scene, current_state->player);
  list_add(current_state->all_characters, current_state->player);
  create_collision(current_state->scene, current_state->player,
                   current_state->water_body, character_hit_water_handler,
                   current_state, 0.0, NULL);
  create_collision(
      current_state->scene, current_state->player, current_state->platform_l,
      character_platform_contact_handler, current_state, 0.0, NULL);

  current_state->enemy1 = make_character_body(
      ENEMY1_START_POS, CHARACTER_SIZE, ENEMY_COLOR_1, TYPE_ENEMY, 1, 10.0);
  scene_add_body(current_state->scene, current_state->enemy1);
  list_add(current_state->all_characters, current_state->enemy1);
  create_collision(current_state->scene, current_state->enemy1,
                   current_state->water_body, character_hit_water_handler,
                   current_state, 0.0, NULL);
  create_collision(
      current_state->scene, current_state->enemy1, current_state->platform_r,
      character_platform_contact_handler, current_state, 0.0, NULL);

  current_state->enemy2 = make_character_body(
      ENEMY2_START_POS, CHARACTER_SIZE, ENEMY_COLOR_2, TYPE_ENEMY, 2, 10.0);
  scene_add_body(current_state->scene, current_state->enemy2);
  list_add(current_state->all_characters, current_state->enemy2);
  create_collision(current_state->scene, current_state->enemy2,
                   current_state->water_body, character_hit_water_handler,
                   current_state, 0.0, NULL);
  create_collision(
      current_state->scene, current_state->enemy2, current_state->platform_r,
      character_platform_contact_handler, current_state, 0.0, NULL);

  scene_add_force_creator(current_state->scene, apply_conditional_gravity,
                          current_state->all_characters,
                          current_state->all_characters, NULL);
  sdl_on_key((key_handler_t)on_key_press);
  return current_state;
}

void update_and_draw_hp_bars(state_t *state) {
  // Player
  vector_t p_pos = body_get_centroid(state->player);
  vector_t p_hp_bg_pos = vec_add(p_pos, (vector_t){0, HP_BAR_Y_OFFSET});
  character_info_t *p_info = (character_info_t *)body_get_info(state->player);
  if (p_info->current_hp > 0) {
    sdl_draw_body(make_generic_rectangle_body(p_hp_bg_pos, HP_BAR_WIDTH,
                                              HP_BAR_HEIGHT, HP_BAR_BG_COLOR,
                                              TYPE_HP_BAR, INFINITY));
    double p_hp_percent = p_info->current_hp / p_info->max_hp;
    double p_fg_width = HP_BAR_WIDTH * p_hp_percent;
    vector_t p_hp_fg_pos =
        vec_add(p_hp_bg_pos, (vector_t){-(HP_BAR_WIDTH - p_fg_width) / 2.0, 0});
    sdl_draw_body(make_generic_rectangle_body(p_hp_fg_pos, p_fg_width,
                                              HP_BAR_HEIGHT, HP_BAR_FG_COLOR,
                                              TYPE_HP_BAR, INFINITY));
  }
  // Enemy 1
  vector_t e1_pos = body_get_centroid(state->enemy1);
  vector_t e1_hp_bg_pos = vec_add(e1_pos, (vector_t){0, HP_BAR_Y_OFFSET});
  character_info_t *e1_info = (character_info_t *)body_get_info(state->enemy1);
  if (e1_info->current_hp > 0) {
    sdl_draw_body(make_generic_rectangle_body(e1_hp_bg_pos, HP_BAR_WIDTH,
                                              HP_BAR_HEIGHT, HP_BAR_BG_COLOR,
                                              TYPE_HP_BAR, INFINITY));
    double e1_hp_percent = e1_info->current_hp / e1_info->max_hp;
    double e1_fg_width = HP_BAR_WIDTH * e1_hp_percent;
    vector_t e1_hp_fg_pos = vec_add(
        e1_hp_bg_pos, (vector_t){-(HP_BAR_WIDTH - e1_fg_width) / 2.0, 0});
    sdl_draw_body(make_generic_rectangle_body(e1_hp_fg_pos, e1_fg_width,
                                              HP_BAR_HEIGHT, HP_BAR_FG_COLOR,
                                              TYPE_HP_BAR, INFINITY));
  }
  // Enemy 2
  vector_t e2_pos = body_get_centroid(state->enemy2);
  vector_t e2_hp_bg_pos = vec_add(e2_pos, (vector_t){0, HP_BAR_Y_OFFSET});
  character_info_t *e2_info = (character_info_t *)body_get_info(state->enemy2);
  if (e2_info->current_hp > 0) {
    sdl_draw_body(make_generic_rectangle_body(e2_hp_bg_pos, HP_BAR_WIDTH,
                                              HP_BAR_HEIGHT, HP_BAR_BG_COLOR,
                                              TYPE_HP_BAR, INFINITY));
    double e2_hp_percent = e2_info->current_hp / e2_info->max_hp;
    double e2_fg_width = HP_BAR_WIDTH * e2_hp_percent;
    vector_t e2_hp_fg_pos = vec_add(
        e2_hp_bg_pos, (vector_t){-(HP_BAR_WIDTH - e2_fg_width) / 2.0, 0});
    sdl_draw_body(make_generic_rectangle_body(e2_hp_fg_pos, e2_fg_width,
                                              HP_BAR_HEIGHT, HP_BAR_FG_COLOR,
                                              TYPE_HP_BAR, INFINITY));
  }
}

bool emscripten_main(state_t *current_state) {
  double dt = time_since_last_tick();

  if (current_state->current_status != GAME_OVER_WATER) {
    if (current_state->current_status == ENEMY1_FIRING) {
      printf("Enemy 1 firing.\n");
      fire_bullet(current_state, current_state->enemy1, current_state->player,
                  TYPE_BULLET_ENEMY1);
      current_state->current_status = ENEMY1_SHOT_ACTIVE;
    } else if (current_state->current_status == ENEMY2_FIRING) {
      printf("Enemy 2 firing.\n");
      fire_bullet(current_state, current_state->enemy2, current_state->player,
                  TYPE_BULLET_ENEMY2);
      current_state->current_status = ENEMY2_SHOT_ACTIVE;
    }
  }

  scene_tick(current_state->scene, dt);

  sdl_clear();
  list_t *assets = asset_get_asset_list();
  for (size_t i = 0; i < list_size(assets); i++) {
    asset_render(list_get(assets, i));
  }
  for (size_t i = 0; i < scene_bodies(current_state->scene); i++) {
    body_t *body_to_draw = scene_get_body(current_state->scene, i);
    if (body_to_draw) {
      // Don't draw HP bar bodies here, we will draw them separately
      body_type_t *info = body_get_info(body_to_draw);
      if (!info || *info != TYPE_HP_BAR) {
        sdl_draw_body(body_to_draw);
      }
    }
  }

  // This will now create temporary bodies for HP bars and draw them
  update_and_draw_hp_bars(current_state);

  sdl_show();

  return false;
}

void emscripten_free(state_t *current_state) {
  list_free(current_state->all_characters);
  scene_free(current_state->scene);
  asset_cache_destroy();
  free(current_state);
}
