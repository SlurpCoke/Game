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
const color_t ENEMY_COLOR = (color_t){0.8, 0.0, 0.0};
const color_t BULLET_COLOR = (color_t){0.9, 0.9, 0.2};
const color_t WATER_COLOR = (color_t){0.2, 0.2, 0.8};
const color_t PLATFORM_COLOR = (color_t){0.5, 0.5, 0.5};
const color_t PLAYER_HIT_COLOR = (color_t){1.0, 0.5, 0.0};
const color_t ENEMY_HIT_COLOR = (color_t){0.5, 0.0, 0.5};
const color_t WATER_DEATH_COLOR = (color_t){0.1, 0.1, 0.1};

// Sizes and Positions
const double PLATFORM_WIDTH = 100.0;
const double PLATFORM_HEIGHT = 20.0;
const double PLAYER_SIZE = 30.0;
const double ENEMY_SIZE = 30.0;
const double BULLET_WIDTH = 20.0;
const double BULLET_HEIGHT = 10.0;
const double WATER_Y_CENTER = 10.0;
const double WATER_HEIGHT = 20.0;

const double PLATFORM_Y = WATER_Y_CENTER + WATER_HEIGHT / 2.0 + 20.0 + PLATFORM_HEIGHT / 2.0;

const vector_t PLATFORM_L_POS = {100, PLATFORM_Y};
const vector_t PLATFORM_R_POS = {MAX_SCREEN_COORDS.x - 100, PLATFORM_Y};

const vector_t PLAYER_START_POS = {PLATFORM_L_POS.x, PLATFORM_L_POS.y + PLATFORM_HEIGHT / 2.0 + PLAYER_SIZE / 2.0 + 0.1};
const vector_t ENEMY_START_POS = {PLATFORM_R_POS.x, PLATFORM_R_POS.y + PLATFORM_HEIGHT / 2.0 + ENEMY_SIZE / 2.0 + 0.1};

const double BULLET_VELOCITY_X = 200.0;
const double GRAVITY_ACCELERATION = 150.0; // Now a global const

const char *BACKGROUND_PATH = "assets/frogger-background.png";

typedef enum {
  WAITING_FOR_PLAYER_SHOT,
  PLAYER_BULLET_FIRED,
  WAITING_FOR_ENEMY_SHOT,
  ENEMY_BULLET_FIRED,
  REMOVING_PLATFORMS,
  WAITING_FOR_FALL,
  SIMULATION_DONE
} game_status_t;

typedef enum {
  TYPE_PLAYER,
  TYPE_ENEMY,
  TYPE_BULLET_PLAYER,
  TYPE_BULLET_ENEMY,
  TYPE_WATER,
  TYPE_PLATFORM,
  TYPE_OTHER
} body_type_t;

struct state {
  scene_t *scene;
  body_t *player;
  body_t *enemy;
  body_t *water_body;
  body_t *platform_l;
  body_t *platform_r;
  game_status_t current_status;
  bool player_is_hit;
  bool enemy_is_hit;
  bool gravity_enabled; // New flag
};

void bullet_hit_target_handler(body_t *bullet, body_t *target, vector_t axis, void *aux, double force_const);
void character_hit_water_handler(body_t *character, body_t *water, vector_t axis, void *aux, double force_const);
void apply_downward_gravity(void *aux_state, list_t *bodies_affected); // aux_state will be state_t*
void character_platform_contact_handler(body_t *character, body_t *platform, vector_t axis, void *aux, double force_const);

body_t *make_rectangle_body(vector_t center, double width, double height, color_t color, body_type_t type_tag, double mass) {
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

body_t *fire_bullet(state_t *current_state, body_t *shooter, vector_t velocity, color_t color, body_type_t bullet_tag, body_t *target_body) {
  vector_t shooter_pos = body_get_centroid(shooter);
  vector_t bullet_start_pos = shooter_pos;
  bullet_start_pos.x += (velocity.x > 0 ? PLAYER_SIZE / 2.0 : -PLAYER_SIZE / 2.0) + (velocity.x > 0 ? BULLET_WIDTH / 2.0 : -BULLET_WIDTH / 2.0) + 5.0;

  body_t *bullet = make_rectangle_body(bullet_start_pos, BULLET_WIDTH, BULLET_HEIGHT, color, bullet_tag, 1.0);
  body_set_velocity(bullet, velocity);
  scene_add_body(current_state->scene, bullet);
  create_collision(current_state->scene, bullet, target_body, bullet_hit_target_handler, current_state, 0.0, NULL);
  return bullet;
}

void on_key_press(char key, key_event_type_t type, double held_time, state_t *current_state) {
  if (type == KEY_PRESSED) {
    if (key == SPACE_BAR && current_state->current_status == WAITING_FOR_PLAYER_SHOT) {
      printf("Player firing.\n");
      fire_bullet(current_state, current_state->player, (vector_t){BULLET_VELOCITY_X, 0}, BULLET_COLOR, TYPE_BULLET_PLAYER, current_state->enemy);
      current_state->current_status = PLAYER_BULLET_FIRED;
    }
  }
}

void bullet_hit_target_handler(body_t *bullet, body_t *target, vector_t axis, void *aux, double force_const) {
  state_t *current_state = (state_t *)aux;
  body_type_t bullet_type = *(body_type_t *)body_get_info(bullet);
  body_type_t target_type = *(body_type_t *)body_get_info(target);

  if (bullet_type == TYPE_BULLET_PLAYER && target_type == TYPE_ENEMY) {
    if (!current_state->enemy_is_hit) {
      printf("Enemy hit by player bullet.\n");
      body_set_color(current_state->enemy, ENEMY_HIT_COLOR);
      current_state->enemy_is_hit = true;
      current_state->current_status = WAITING_FOR_ENEMY_SHOT;
    }
  } else if (bullet_type == TYPE_BULLET_ENEMY && target_type == TYPE_PLAYER) {
    if (!current_state->player_is_hit) {
      printf("Player hit by enemy bullet.\n");
      body_set_color(current_state->player, PLAYER_HIT_COLOR);
      current_state->player_is_hit = true;
      current_state->current_status = REMOVING_PLATFORMS;
    }
  }
  body_remove(bullet);
}

void character_hit_water_handler(body_t *character, body_t *water, vector_t axis, void *aux, double force_const) {
  body_type_t char_type = *(body_type_t *)body_get_info(character);
  color_t current_color = body_get_color(character);

  if (current_color.red != WATER_DEATH_COLOR.red || current_color.green != WATER_DEATH_COLOR.green || current_color.blue != WATER_DEATH_COLOR.blue) {
    if (char_type == TYPE_PLAYER) {
      printf("Player touched water.\n");
    } else if (char_type == TYPE_ENEMY) {
      printf("Enemy touched water.\n");
    }
    body_set_color(character, WATER_DEATH_COLOR);
    body_set_velocity(character, VEC_ZERO);
  }
}

// Modified to use state_t* as aux and check gravity_enabled flag
void apply_downward_gravity(void *aux_state, list_t *bodies_affected) {
    state_t *current_state = (state_t *)aux_state;
    if (!current_state->gravity_enabled) { // Check the flag
        return;
    }

    for (size_t i = 0; i < list_size(bodies_affected); i++) {
        body_t *body = (body_t *)list_get(bodies_affected, i);
        if (body_get_mass(body) != INFINITY) {
            vector_t gravity_force = {0, -body_get_mass(body) * GRAVITY_ACCELERATION};
            body_add_force(body, gravity_force);
        }
    }
}

void character_platform_contact_handler(body_t *character, body_t *platform, vector_t axis, void *aux, double force_const) {
    state_t *current_state = (state_t *)aux; // aux is now state_t*

    // Only apply this handler if gravity is NOT yet enabled OR if the platform still exists
    // (When gravity is enabled and platforms are gone, this handler shouldn't interfere with falling)
    if (current_state->gravity_enabled && ( (platform == current_state->platform_l && body_is_removed(current_state->platform_l)) || (platform == current_state->platform_r && body_is_removed(current_state->platform_r)) ) ) {
        return;
    }
    
    vector_t char_vel = body_get_velocity(character);
    vector_t char_pos = body_get_centroid(character);
    vector_t platform_pos = body_get_centroid(platform);

    double char_half_height = PLAYER_SIZE / 2.0; // Assuming player and enemy are same size
    body_type_t char_type = *(body_type_t*)body_get_info(character);
    if (char_type == TYPE_ENEMY) {
         char_half_height = ENEMY_SIZE / 2.0;
    }
    double platform_half_height = PLATFORM_HEIGHT / 2.0;

    double char_bottom = char_pos.y - char_half_height;
    double platform_top = platform_pos.y + platform_half_height;

    if (axis.y > 0.5 && (char_bottom < platform_top + 1.5)) { // Increased tolerance slightly
        // If gravity is not yet on, or if it is on but we are still "resting" (e.g. small downward velocity)
        if (!current_state->gravity_enabled || char_vel.y < 1.0) { // Allow small jitter if gravity is on
             if (char_vel.y < 0) {
                body_set_velocity(character, (vector_t){char_vel.x, 0.0});
             }
            body_set_centroid(character, (vector_t){char_pos.x, platform_top + char_half_height + 0.01}); // Tiny offset
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
  current_state->player_is_hit = false;
  current_state->enemy_is_hit = false;
  current_state->gravity_enabled = false; // Initialize gravity as off
  current_state->platform_l = NULL;
  current_state->platform_r = NULL;

  asset_make_image(BACKGROUND_PATH, (SDL_Rect){(int)MIN_SCREEN_COORDS.x, (int)MIN_SCREEN_COORDS.y, (int)MAX_SCREEN_COORDS.x, (int)MAX_SCREEN_COORDS.y});

  current_state->water_body = make_rectangle_body((vector_t){MAX_SCREEN_COORDS.x / 2.0, WATER_Y_CENTER}, MAX_SCREEN_COORDS.x, WATER_HEIGHT, WATER_COLOR, TYPE_WATER, INFINITY);
  scene_add_body(current_state->scene, current_state->water_body);

  current_state->platform_l = make_rectangle_body(PLATFORM_L_POS, PLATFORM_WIDTH, PLATFORM_HEIGHT, PLATFORM_COLOR, TYPE_PLATFORM, INFINITY);
  scene_add_body(current_state->scene, current_state->platform_l);
  current_state->platform_r = make_rectangle_body(PLATFORM_R_POS, PLATFORM_WIDTH, PLATFORM_HEIGHT, PLATFORM_COLOR, TYPE_PLATFORM, INFINITY);
  scene_add_body(current_state->scene, current_state->platform_r);

  current_state->player = make_rectangle_body(PLAYER_START_POS, PLAYER_SIZE, PLAYER_SIZE, PLAYER_COLOR, TYPE_PLAYER, 10.0);
  scene_add_body(current_state->scene, current_state->player);
  create_collision(current_state->scene, current_state->player, current_state->water_body, character_hit_water_handler, current_state, 0.0, NULL);
  create_collision(current_state->scene, current_state->player, current_state->platform_l, character_platform_contact_handler, current_state, 0.0, NULL);


  current_state->enemy = make_rectangle_body(ENEMY_START_POS, ENEMY_SIZE, ENEMY_SIZE, ENEMY_COLOR, TYPE_ENEMY, 10.0);
  scene_add_body(current_state->scene, current_state->enemy);
  create_collision(current_state->scene, current_state->enemy, current_state->water_body, character_hit_water_handler, current_state, 0.0, NULL);
  create_collision(current_state->scene, current_state->enemy, current_state->platform_r, character_platform_contact_handler, current_state, 0.0, NULL);

  list_t *gravity_bodies = list_init(2, NULL);
  list_add(gravity_bodies, current_state->player);
  list_add(gravity_bodies, current_state->enemy);

  // Pass current_state as aux to gravity, freer is NULL as state is managed elsewhere
  scene_add_force_creator(current_state->scene, apply_downward_gravity, current_state, gravity_bodies, NULL);

  sdl_on_key((key_handler_t)on_key_press);
  return current_state;
}

bool emscripten_main(state_t *current_state) {
  double dt = time_since_last_tick();

  if (current_state->current_status == WAITING_FOR_ENEMY_SHOT) {
    printf("Enemy firing.\n");
    fire_bullet(current_state, current_state->enemy, (vector_t){-BULLET_VELOCITY_X, 0}, BULLET_COLOR, TYPE_BULLET_ENEMY, current_state->player);
    current_state->current_status = ENEMY_BULLET_FIRED;
  } else if (current_state->current_status == REMOVING_PLATFORMS) {
    printf("Removing platforms & Enabling Gravity.\n");
    if (current_state->platform_l && !body_is_removed(current_state->platform_l)) {
      body_remove(current_state->platform_l);
    }
    if (current_state->platform_r && !body_is_removed(current_state->platform_r)) {
      body_remove(current_state->platform_r);
    }
    current_state->gravity_enabled = true; // Enable gravity HERE
    current_state->current_status = WAITING_FOR_FALL;
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
      sdl_draw_body(body_to_draw);
    }
  }
  sdl_show();

  return false;
}

void emscripten_free(state_t *current_state) {
  scene_free(current_state->scene);
  asset_cache_destroy();
  free(current_state);
}
