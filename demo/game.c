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
const color_t PLAYER_COLOR = (color_t){0.0, 0.8, 0.0}; // Green
const color_t ENEMY_COLOR = (color_t){0.8, 0.0, 0.0};  // Red
const color_t BULLET_COLOR = (color_t){0.9, 0.9, 0.2}; // Yellow
const color_t WATER_COLOR = (color_t){0.2, 0.2, 0.8};  // Blue
const color_t PLATFORM_COLOR = (color_t){0.5, 0.5, 0.5}; // Grey
const color_t PLAYER_HIT_COLOR = (color_t){1.0, 0.5, 0.0}; // Orange
const color_t ENEMY_HIT_COLOR = (color_t){0.5, 0.0, 0.5};  // Purple
const color_t WATER_DEATH_COLOR = (color_t){0.1, 0.1, 0.1}; // Dark Grey/Black

// Sizes and Positions
const double PLATFORM_WIDTH = 100.0;
const double PLATFORM_HEIGHT = 20.0;
const double PLAYER_SIZE = 30.0; // Assuming square
const double ENEMY_SIZE = 30.0;  // Assuming square
const double BULLET_WIDTH = 20.0;
const double BULLET_HEIGHT = 10.0;
const double WATER_INITIAL_Y_CENTER = 10.0; // Center of water rectangle
const double WATER_HEIGHT = 20.0;           // Height of water rectangle
const double WATER_FINAL_Y_CENTER = 70.0; // Water will rise to this Y center
const double WATER_RISE_SPEED = 10.0;     // Pixels per second

const vector_t PLAYER_START_POS = {100, PLATFORM_HEIGHT + WATER_INITIAL_Y_CENTER + PLAYER_SIZE / 2.0 + 10};
const vector_t ENEMY_START_POS = {MAX_SCREEN_COORDS.x - 100, PLATFORM_HEIGHT + WATER_INITIAL_Y_CENTER + ENEMY_SIZE / 2.0 + 10};
const vector_t PLATFORM_L_POS = {PLAYER_START_POS.x, PLAYER_START_POS.y - PLAYER_SIZE / 2.0 - PLATFORM_HEIGHT / 2.0};
const vector_t PLATFORM_R_POS = {ENEMY_START_POS.x, ENEMY_START_POS.y - ENEMY_SIZE / 2.0 - PLATFORM_HEIGHT / 2.0};

const double BULLET_VELOCITY_X = 200.0;

// Asset paths
const char *BACKGROUND_PATH = "assets/frogger-background.png"; // Kept from original

// Game States
typedef enum {
  WAITING_FOR_PLAYER_SHOT,
  PLAYER_BULLET_FIRED,
  WAITING_FOR_ENEMY_SHOT,
  ENEMY_BULLET_FIRED,
  RAISING_WATER,
  SIMULATION_DONE
} game_status_t;

// Body Info (simple type tagging)
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
  game_status_t current_status;
  bool player_is_hit;
  bool enemy_is_hit;
};

// Forward declarations for collision handlers
void bullet_hit_target_handler(body_t *bullet, body_t *target, vector_t axis, void *aux, double force_const);
void character_hit_water_handler(body_t *character, body_t *water, vector_t axis, void *aux, double force_const);

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
  // Adjust start position slightly in front of shooter based on velocity direction
  bullet_start_pos.x += (velocity.x > 0 ? PLAYER_SIZE / 2 : -PLAYER_SIZE / 2) + (velocity.x > 0 ? BULLET_WIDTH/2 : -BULLET_WIDTH/2) + 5;


  body_t *bullet = make_rectangle_body(bullet_start_pos, BULLET_WIDTH, BULLET_HEIGHT, color, bullet_tag, 1.0); // Mass 1 for bullets
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
      current_state->current_status = WAITING_FOR_ENEMY_SHOT; // Trigger enemy shot
    }
  } else if (bullet_type == TYPE_BULLET_ENEMY && target_type == TYPE_PLAYER) {
    if (!current_state->player_is_hit) {
      printf("Player hit by enemy bullet.\n");
      body_set_color(current_state->player, PLAYER_HIT_COLOR);
      current_state->player_is_hit = true;
      current_state->current_status = RAISING_WATER; // Trigger water rise
    }
  }
  body_remove(bullet); // Remove bullet on any collision it's registered for
}

void character_hit_water_handler(body_t *character, body_t *water, vector_t axis, void *aux, double force_const) {
  state_t *current_state = (state_t *)aux;
  body_type_t char_type = *(body_type_t *)body_get_info(character);
  
  // Check current color to prevent repeated changes if already "dead"
  color_t current_color = body_get_color(character);
  // *** FIXED THE ERROR HERE ***
  if(current_color.red != WATER_DEATH_COLOR.red || current_color.green != WATER_DEATH_COLOR.green || current_color.blue != WATER_DEATH_COLOR.blue){
    if (char_type == TYPE_PLAYER) {
        printf("Player touched water.\n");
    } else if (char_type == TYPE_ENEMY) {
        printf("Enemy touched water.\n");
    }
    body_set_color(character, WATER_DEATH_COLOR);
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

  // Load background asset
  asset_make_image(BACKGROUND_PATH, (SDL_Rect){(int)MIN_SCREEN_COORDS.x, (int)MIN_SCREEN_COORDS.y, (int)MAX_SCREEN_COORDS.x, (int)MAX_SCREEN_COORDS.y});

  // Create water
  current_state->water_body = make_rectangle_body((vector_t){MAX_SCREEN_COORDS.x / 2, WATER_INITIAL_Y_CENTER}, MAX_SCREEN_COORDS.x, WATER_HEIGHT, WATER_COLOR, TYPE_WATER, INFINITY);
  scene_add_body(current_state->scene, current_state->water_body);

  // Create platforms
  body_t *platform_l = make_rectangle_body(PLATFORM_L_POS, PLATFORM_WIDTH, PLATFORM_HEIGHT, PLATFORM_COLOR, TYPE_PLATFORM, INFINITY);
  scene_add_body(current_state->scene, platform_l);
  body_t *platform_r = make_rectangle_body(PLATFORM_R_POS, PLATFORM_WIDTH, PLATFORM_HEIGHT, PLATFORM_COLOR, TYPE_PLATFORM, INFINITY);
  scene_add_body(current_state->scene, platform_r);

  // Create player
  current_state->player = make_rectangle_body(PLAYER_START_POS, PLAYER_SIZE, PLAYER_SIZE, PLAYER_COLOR, TYPE_PLAYER, 10.0); // Mass 10 for player
  scene_add_body(current_state->scene, current_state->player);
  create_collision(current_state->scene, current_state->player, current_state->water_body, character_hit_water_handler, current_state, 0.0, NULL);


  // Create enemy
  current_state->enemy = make_rectangle_body(ENEMY_START_POS, ENEMY_SIZE, ENEMY_SIZE, ENEMY_COLOR, TYPE_ENEMY, 10.0); // Mass 10 for enemy
  scene_add_body(current_state->scene, current_state->enemy);
  create_collision(current_state->scene, current_state->enemy, current_state->water_body, character_hit_water_handler, current_state, 0.0, NULL);


  sdl_on_key((key_handler_t)on_key_press);
  return current_state;
}

bool emscripten_main(state_t *current_state) {
  double dt = time_since_last_tick();

  // Game logic based on state
  if (current_state->current_status == WAITING_FOR_ENEMY_SHOT) {
      printf("Enemy firing.\n");
      fire_bullet(current_state, current_state->enemy, (vector_t){-BULLET_VELOCITY_X, 0}, BULLET_COLOR, TYPE_BULLET_ENEMY, current_state->player);
      current_state->current_status = ENEMY_BULLET_FIRED;
  }
  else if (current_state->current_status == RAISING_WATER) {
    vector_t water_pos = body_get_centroid(current_state->water_body);
    if (water_pos.y < WATER_FINAL_Y_CENTER) {
      water_pos.y += WATER_RISE_SPEED * dt;
      if(water_pos.y > WATER_FINAL_Y_CENTER) water_pos.y = WATER_FINAL_Y_CENTER;
      body_set_centroid(current_state->water_body, water_pos);
      // Re-register collisions as body position changes; alternatively, ensure collision system handles dynamic positions.
      // For simplicity, we assume the collision system handles this or we re-evaluate.
      // (Re-adding collision handlers every frame is inefficient; the system should handle overlaps continuously)
      // If not automatically handled, we might need to remove old collision handlers and add new ones,
      // or the collision system is designed to continuously check.
      // For this demo, we set it once in init and trust the collision system.
    } else {
        if (current_state->current_status != SIMULATION_DONE) {
            printf("Water rise complete. Simulation ended.\n");
            current_state->current_status = SIMULATION_DONE;
        }
    }
  }

  scene_tick(current_state->scene, dt);

  sdl_clear();
  list_t *assets = asset_get_asset_list();
  for (size_t i = 0; i < list_size(assets); i++) {
    asset_render(list_get(assets, i)); // Render background
  }
  // Render bodies directly if no assets are associated or for debugging
   for (size_t i = 0; i < scene_bodies(current_state->scene); i++) {
       body_t *body_to_draw = scene_get_body(current_state->scene, i);
       // Skip drawing if it's the background asset's effective body (if any special handling)
       // or draw all bodies for debugging.
       sdl_draw_body(body_to_draw);
   }
  sdl_show();

  return false; // Game loop continues
}

void emscripten_free(state_t *current_state) {
  scene_free(current_state->scene);
  asset_cache_destroy(); // Destroy cache after scene, as scene might hold assets
  free(current_state);
}
