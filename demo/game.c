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
const color_t PLAYER_HIT_COLOR = (color_t){1.0, 0.5, 0.0};
const color_t ENEMY_HIT_COLOR = (color_t){0.5, 0.0, 0.5};
const color_t WATER_DEATH_COLOR = (color_t){0.1, 0.1, 0.1};

// Sizes and Positions
const double PLATFORM_WIDTH = 150.0; 
const double PLATFORM_HEIGHT = 20.0;
const double CHARACTER_SIZE = 30.0; 
const double BULLET_WIDTH = 20.0; // Correct bullet dimensions
const double BULLET_HEIGHT = 10.0;
const double WATER_Y_CENTER = 10.0;
const double WATER_HEIGHT = 20.0;

const double PLATFORM_Y = WATER_Y_CENTER + WATER_HEIGHT / 2.0 + 20.0 + PLATFORM_HEIGHT / 2.0;

const vector_t PLATFORM_L_POS = {150, PLATFORM_Y};
const vector_t PLATFORM_R_POS = {MAX_SCREEN_COORDS.x - 150, PLATFORM_Y};

const vector_t PLAYER_START_POS = {PLATFORM_L_POS.x + 50, PLATFORM_L_POS.y + PLATFORM_HEIGHT / 2.0 + CHARACTER_SIZE / 2.0 + 0.1};
const vector_t ENEMY1_START_POS = {PLATFORM_R_POS.x - CHARACTER_SIZE / 1.5, PLATFORM_R_POS.y + PLATFORM_HEIGHT / 2.0 + CHARACTER_SIZE / 2.0 + 0.1}; // Adjusted for spacing
const vector_t ENEMY2_START_POS = {PLATFORM_R_POS.x + CHARACTER_SIZE / 1.5, PLATFORM_R_POS.y + PLATFORM_HEIGHT / 2.0 + CHARACTER_SIZE / 2.0 + 0.1}; // Adjusted for spacing

const double BULLET_VELOCITY_X = 250.0;
const double GRAVITY_ACCELERATION = 250.0;
const double KNOCKBACK_BASE_VELOCITY_X = 120.0;
const double KNOCKBACK_BASE_VELOCITY_Y = 100.0; 

const char *BACKGROUND_PATH = "assets/frogger-background.png";

typedef enum {
    TYPE_PLAYER,
    TYPE_ENEMY,
    TYPE_BULLET_PLAYER,
    TYPE_BULLET_ENEMY1,
    TYPE_BULLET_ENEMY2,
    TYPE_WATER,
    TYPE_PLATFORM
} body_type_t;

typedef struct {
    body_type_t type;
    bool affected_by_gravity;
    bool is_knocked_back;
    int id; 
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


struct state {
  scene_t *scene;
  body_t *player;
  body_t *enemy1;
  body_t *enemy2;
  body_t *water_body;
  body_t *platform_l;
  body_t *platform_r;
  game_status_t current_status;
};

void bullet_hit_target_handler(body_t *bullet, body_t *target, vector_t axis, void *aux, double force_const);
void character_hit_water_handler(body_t *character, body_t *water, vector_t axis, void *aux, double force_const);
void apply_conditional_gravity(void *aux_state, list_t *all_characters);
void character_platform_contact_handler(body_t *character, body_t *platform, vector_t axis, void *aux, double force_const);

// Generic rectangle body maker (can be used for platforms, water)
body_t *make_generic_rectangle_body(vector_t center, double width, double height, color_t color, body_type_t type_tag, double mass) {
    list_t *shape = list_init(4, free);
    vector_t *v1 = malloc(sizeof(vector_t)); *v1 = (vector_t){-width/2, -height/2}; list_add(shape, v1);
    vector_t *v2 = malloc(sizeof(vector_t)); *v2 = (vector_t){+width/2, -height/2}; list_add(shape, v2);
    vector_t *v3 = malloc(sizeof(vector_t)); *v3 = (vector_t){+width/2, +height/2}; list_add(shape, v3);
    vector_t *v4 = malloc(sizeof(vector_t)); *v4 = (vector_t){-width/2, +height/2}; list_add(shape, v4);

    // For platforms/water, a simple body_type_t tag is enough for info
    body_type_t *info = malloc(sizeof(body_type_t)); 
    *info = type_tag;
    
    body_t *rect_body = body_init_with_info(shape, mass, color, info, free);
    body_set_centroid(rect_body, center);
    return rect_body;
}

// Specific maker for characters (player, enemies) that use character_info_t
body_t *make_character_body(vector_t center, double size, color_t color, body_type_t type, int id, double mass) {
    list_t *shape = list_init(4, free);
    vector_t *v1 = malloc(sizeof(vector_t)); *v1 = (vector_t){-size/2, -size/2}; list_add(shape, v1);
    vector_t *v2 = malloc(sizeof(vector_t)); *v2 = (vector_t){+size/2, -size/2}; list_add(shape, v2);
    vector_t *v3 = malloc(sizeof(vector_t)); *v3 = (vector_t){+size/2, +size/2}; list_add(shape, v3);
    vector_t *v4 = malloc(sizeof(vector_t)); *v4 = (vector_t){-size/2, +size/2}; list_add(shape, v4);

    character_info_t *info = malloc(sizeof(character_info_t));
    info->type = type;
    info->affected_by_gravity = false; 
    info->is_knocked_back = false;
    info->id = id;

    body_t *char_body = body_init_with_info(shape, mass, color, info, free);
    body_set_centroid(char_body, center);
    return char_body;
}

// New function for projectiles
body_t *make_projectile_body(vector_t center, double width, double height, color_t color, body_type_t type, int shooter_id, double mass) {
    list_t *shape = list_init(4, free);
    vector_t *v1 = malloc(sizeof(vector_t)); *v1 = (vector_t){-width/2, -height/2}; list_add(shape, v1);
    vector_t *v2 = malloc(sizeof(vector_t)); *v2 = (vector_t){+width/2, -height/2}; list_add(shape, v2);
    vector_t *v3 = malloc(sizeof(vector_t)); *v3 = (vector_t){+width/2, +height/2}; list_add(shape, v3);
    vector_t *v4 = malloc(sizeof(vector_t)); *v4 = (vector_t){-width/2, +height/2}; list_add(shape, v4);

    character_info_t *info = malloc(sizeof(character_info_t)); // Using character_info_t for type consistency
    info->type = type;
    info->affected_by_gravity = false; // Bullets typically aren't affected by gravity
    info->is_knocked_back = false;     // Bullets don't get knocked back
    info->id = shooter_id;             // Store shooter ID if needed, or -1 for generic bullet

    body_t *projectile = body_init_with_info(shape, mass, color, info, free);
    body_set_centroid(projectile, center);
    return projectile;
}


body_t *fire_bullet(state_t *current_state, body_t *shooter, body_t *target_to_aim_at, body_type_t bullet_tag) {
    vector_t shooter_pos = body_get_centroid(shooter);
    vector_t target_pos = body_get_centroid(target_to_aim_at);
    vector_t bullet_start_pos = shooter_pos;
    character_info_t* shooter_info = (character_info_t*)body_get_info(shooter);


    vector_t fire_direction = vec_subtract(target_pos, shooter_pos);
    double dir_x = (fire_direction.x == 0) ? ((bullet_tag == TYPE_BULLET_PLAYER) ? 1.0 : -1.0) : ((fire_direction.x > 0) ? 1.0 : -1.0) ;
    
    bullet_start_pos.x += dir_x * (CHARACTER_SIZE / 2.0 + BULLET_WIDTH / 2.0 + 5.0); // Use CHARACTER_SIZE for shooter offset
    vector_t bullet_velocity = {dir_x * BULLET_VELOCITY_X, 0}; 

    // Use the new make_projectile_body function
    body_t *bullet = make_projectile_body(bullet_start_pos, BULLET_WIDTH, BULLET_HEIGHT, BULLET_COLOR, bullet_tag, shooter_info ? shooter_info->id : -1, 1.0);
    
    body_set_velocity(bullet, bullet_velocity);
    scene_add_body(current_state->scene, bullet);

    // Bullets from player can hit enemy1 or enemy2
    // Bullets from enemies can hit player
    if (bullet_tag == TYPE_BULLET_PLAYER) {
         create_collision(current_state->scene, bullet, current_state->enemy1, bullet_hit_target_handler, current_state, 0.0, NULL);
         create_collision(current_state->scene, bullet, current_state->enemy2, bullet_hit_target_handler, current_state, 0.0, NULL);
    } else if (bullet_tag == TYPE_BULLET_ENEMY1 || bullet_tag == TYPE_BULLET_ENEMY2) {
         create_collision(current_state->scene, bullet, current_state->player, bullet_hit_target_handler, current_state, 0.0, NULL);
    }
    return bullet;
}

void on_key_press(char key, key_event_type_t type, double held_time, state_t *current_state) {
  if (type == KEY_PRESSED && current_state->current_status != GAME_OVER_WATER) { // Prevent actions if game over
    if (key == SPACE_BAR && current_state->current_status == WAITING_FOR_PLAYER_SHOT) {
      printf("Player firing.\n");
      // Player aims at enemy1 by default for this test, or closest if more complex
      fire_bullet(current_state, current_state->player, current_state->enemy1, TYPE_BULLET_PLAYER);
      current_state->current_status = PLAYER_SHOT_ACTIVE;
    }
  }
}

void bullet_hit_target_handler(body_t *bullet, body_t *target, vector_t axis, void *aux, double force_const) {
    state_t *current_state = (state_t *)aux;
    if (current_state->current_status == GAME_OVER_WATER) {
        body_remove(bullet); // Remove bullet even if game is over to clean up
        return;
    }

    character_info_t *bullet_char_info = (character_info_t *)body_get_info(bullet); 
    character_info_t *target_char_info = (character_info_t *)body_get_info(target);

    if (!bullet_char_info || !target_char_info) {
        body_remove(bullet); // Invalid hit, remove bullet
        return;
    }

    body_type_t bullet_type = bullet_char_info->type;
    body_type_t target_type = target_char_info->type;

    bool valid_hit = false;

    if (bullet_type == TYPE_BULLET_PLAYER && (target_type == TYPE_ENEMY)) {
        printf("Enemy ID %d hit by player bullet.\n", target_char_info->id);
        body_set_color(target, ENEMY_HIT_COLOR); 
        valid_hit = true;
        current_state->current_status = ENEMY1_FIRING; 
    } else if ((bullet_type == TYPE_BULLET_ENEMY1 || bullet_type == TYPE_BULLET_ENEMY2) && target_type == TYPE_PLAYER) {
        printf("Player hit by enemy bullet type %d.\n", bullet_type);
        body_set_color(target, PLAYER_HIT_COLOR);
        valid_hit = true;
        if (bullet_type == TYPE_BULLET_ENEMY1) {
            current_state->current_status = ENEMY2_FIRING; 
        } else { 
            current_state->current_status = WAITING_FOR_PLAYER_SHOT; 
        }
    }

    if (valid_hit) {
        target_char_info->affected_by_gravity = true;
        target_char_info->is_knocked_back = true;
        vector_t bullet_vel = body_get_velocity(bullet);
        // Knockback direction is same as bullet's X, Y is fixed upward
        double knock_dir_x = (bullet_vel.x > 0) ? 1.0 : -1.0; 
        body_set_velocity(target, (vector_t){knock_dir_x * KNOCKBACK_BASE_VELOCITY_X, KNOCKBACK_BASE_VELOCITY_Y});
    }
    body_remove(bullet);
}


void character_hit_water_handler(body_t *character, body_t *water, vector_t axis, void *aux, double force_const) {
  state_t *current_state = (state_t *)aux;
  character_info_t *char_info = (character_info_t *)body_get_info(character);
  
  if (!char_info || current_state->current_status == GAME_OVER_WATER) return;

  color_t current_color = body_get_color(character);
  // Compare with a small tolerance for floating point colors if necessary
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

void apply_conditional_gravity(void *aux_unused, list_t *all_characters) {
    for (size_t i = 0; i < list_size(all_characters); i++) {
        body_t *body = (body_t *)list_get(all_characters, i);
        if(body_is_removed(body)) continue; // Skip removed bodies

        character_info_t *info = (character_info_t *)body_get_info(body);
        
        if (info && (info->type == TYPE_PLAYER || info->type == TYPE_ENEMY)) {
            if (info->affected_by_gravity && body_get_mass(body) != INFINITY) {
                vector_t gravity_force = {0, -body_get_mass(body) * GRAVITY_ACCELERATION};
                body_add_force(body, gravity_force);
            }
        }
    }
}

void character_platform_contact_handler(body_t *character, body_t *platform, vector_t axis, void *aux, double force_const) {
    state_t *current_state = (state_t*)aux;
    if(current_state->current_status == GAME_OVER_WATER) return;

    character_info_t *char_info = (character_info_t *)body_get_info(character);
    if (!char_info || (char_info->type != TYPE_PLAYER && char_info->type != TYPE_ENEMY)) return;

    vector_t char_pos = body_get_centroid(character);
    vector_t platform_pos = body_get_centroid(platform);
    double char_half_height = CHARACTER_SIZE / 2.0;
    double platform_half_height = PLATFORM_HEIGHT / 2.0;
    double char_bottom = char_pos.y - char_half_height;
    double platform_top = platform_pos.y + platform_half_height;

    if (axis.y > 0.7 && char_bottom <= platform_top + 2.0) { // Increased tolerance slightly
        if (char_info->is_knocked_back) {
            printf("Character ID %d landed on platform after knockback.\n", char_info->id);
            body_set_velocity(character, VEC_ZERO);
            char_info->is_knocked_back = false;
            char_info->affected_by_gravity = false; 
        }
        
        if (!char_info->affected_by_gravity) { // If not supposed to be falling by gravity
             vector_t current_vel = body_get_velocity(character);
             if(current_vel.y < 0) body_set_velocity(character, (vector_t){current_vel.x, 0.0});
             body_set_centroid(character, (vector_t){char_pos.x, platform_top + char_half_height + 0.01}); 
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

  asset_make_image(BACKGROUND_PATH, (SDL_Rect){0,0, (int)MAX_SCREEN_COORDS.x, (int)MAX_SCREEN_COORDS.y});

  current_state->water_body = make_generic_rectangle_body((vector_t){MAX_SCREEN_COORDS.x / 2.0, WATER_Y_CENTER}, MAX_SCREEN_COORDS.x, WATER_HEIGHT, WATER_COLOR, TYPE_WATER, INFINITY);
  scene_add_body(current_state->scene, current_state->water_body);

  current_state->platform_l = make_generic_rectangle_body(PLATFORM_L_POS, PLATFORM_WIDTH, PLATFORM_HEIGHT, PLATFORM_COLOR, TYPE_PLATFORM, INFINITY);
  scene_add_body(current_state->scene, current_state->platform_l);
  current_state->platform_r = make_generic_rectangle_body(PLATFORM_R_POS, PLATFORM_WIDTH, PLATFORM_HEIGHT, PLATFORM_COLOR, TYPE_PLATFORM, INFINITY); // One platform for both enemies
  scene_add_body(current_state->scene, current_state->platform_r);

  current_state->player = make_character_body(PLAYER_START_POS, CHARACTER_SIZE, PLAYER_COLOR, TYPE_PLAYER, 0, 10.0);
  scene_add_body(current_state->scene, current_state->player);
  create_collision(current_state->scene, current_state->player, current_state->water_body, character_hit_water_handler, current_state, 0.0, NULL);
  create_collision(current_state->scene, current_state->player, current_state->platform_l, character_platform_contact_handler, current_state, 0.0, NULL);

  current_state->enemy1 = make_character_body(ENEMY1_START_POS, CHARACTER_SIZE, ENEMY_COLOR_1, TYPE_ENEMY, 1, 10.0);
  scene_add_body(current_state->scene, current_state->enemy1);
  create_collision(current_state->scene, current_state->enemy1, current_state->water_body, character_hit_water_handler, current_state, 0.0, NULL);
  create_collision(current_state->scene, current_state->enemy1, current_state->platform_r, character_platform_contact_handler, current_state, 0.0, NULL);
  
  current_state->enemy2 = make_character_body(ENEMY2_START_POS, CHARACTER_SIZE, ENEMY_COLOR_2, TYPE_ENEMY, 2, 10.0);
  scene_add_body(current_state->scene, current_state->enemy2);
  create_collision(current_state->scene, current_state->enemy2, current_state->water_body, character_hit_water_handler, current_state, 0.0, NULL);
  create_collision(current_state->scene, current_state->enemy2, current_state->platform_r, character_platform_contact_handler, current_state, 0.0, NULL);

  list_t *all_characters = list_init(3, NULL); 
  list_add(all_characters, current_state->player);
  list_add(all_characters, current_state->enemy1);
  list_add(all_characters, current_state->enemy2);
  scene_add_force_creator(current_state->scene, apply_conditional_gravity, NULL, all_characters, NULL);

  sdl_on_key((key_handler_t)on_key_press);
  return current_state;
}

bool emscripten_main(state_t *current_state) {
  double dt = time_since_last_tick();

  if (current_state->current_status != GAME_OVER_WATER) {
    if (current_state->current_status == ENEMY1_FIRING) {
        printf("Enemy 1 firing.\n");
        fire_bullet(current_state, current_state->enemy1, current_state->player, TYPE_BULLET_ENEMY1);
        current_state->current_status = ENEMY1_SHOT_ACTIVE;
    } else if (current_state->current_status == ENEMY2_FIRING) {
        printf("Enemy 2 firing.\n");
        fire_bullet(current_state, current_state->enemy2, current_state->player, TYPE_BULLET_ENEMY2);
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
