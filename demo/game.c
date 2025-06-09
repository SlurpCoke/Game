#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_mouse.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/asset.h"
#include "../include/asset_cache.h"
#include "../include/body.h"
#include "../include/collision.h"
#include "../include/color.h"
#include "../include/forces.h"
#include "../include/list.h"
#include "../include/scene.h"
#include "../include/sdl_wrapper.h"
#include "../include/state.h"
#include "../include/vector.h"

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
const color_t WHITE = {1, 1, 1};

// Sizes and Positions
const double PLATFORM_WIDTH = 150.0;
const double PLATFORM_HEIGHT = 20.0;
const double CHARACTER_SIZE = 80.0;
const double BULLET_WIDTH = 20.0;
const double BULLET_HEIGHT = 10.0;
const double WATER_Y_CENTER = 10.0;
const double WATER_HEIGHT = 20.0;
const double HP_BAR_WIDTH = 40.0;
const double HP_BAR_HEIGHT = 5.0;
const double HP_BAR_Y_OFFSET = 25.0;
const size_t CIRC_NPOINTS = 20;

const double PLATFORM_Y =
    WATER_Y_CENTER + WATER_HEIGHT / 2.0 + 20.0 + PLATFORM_HEIGHT / 2.0;
const vector_t PLATFORM_L_POS = {150, PLATFORM_Y};
const vector_t PLATFORM_R_POS = {MAX_SCREEN_COORDS.x - 150, PLATFORM_Y};
const vector_t PLAYER_START_POS = {PLATFORM_L_POS.x - 10,
                                   PLATFORM_L_POS.y + PLATFORM_HEIGHT / 2.0 +
                                       CHARACTER_SIZE / 2.0 + 0.1};
const vector_t ENEMY1_START_POS = {PLATFORM_R_POS.x - CHARACTER_SIZE / 1.5,
                                   PLATFORM_R_POS.y + PLATFORM_HEIGHT / 2.0 +
                                       CHARACTER_SIZE / 2.0 + 0.1};
const vector_t ENEMY2_START_POS = {PLATFORM_R_POS.x + CHARACTER_SIZE / 1.5,
                                   PLATFORM_R_POS.y + PLATFORM_HEIGHT / 2.0 +
                                       CHARACTER_SIZE / 2.0 + 0.1};

// Visualization
const size_t N_DOTS = 20;
const double DOT_RADIUS = 4;
const double DOTS_SEP_DT = 0.2;
const double MOUSE_SCALE = 1.5;

// Audio File Paths
const char *BACKGROUND_MUSIC_PATH = "assets/calm_pirate.wav";
const char *LOW_HP_MUSIC_PATH = "assets/pirate_music.wav";

// PNG File Paths
const char *PLAYER1_PATH = "assets/player_1.png";
const char *ENEMY_PATH = "assets/enemy.png";

// Physics & Game Constants
const double BULLET_VELOCITY = 250.0; // per axis
const double GRAVITY_ACCELERATION = 250.0;
const double KNOCKBACK_BASE_VELOCITY_X = 40.0;
const double KNOCKBACK_BASE_VELOCITY_Y = 100.0;
const double MAX_HEALTH = 100.0;
const double SHOT_DELAY_TIME = 1.0;
const double LOW_HP_THRESHOLD = 50.0;
const double BULLET_WEIGHT = 5;

const char *BACKGROUND_PATH = "assets/frogger-background.png";

typedef enum {
  TYPE_PLAYER,
  TYPE_ENEMY,
  TYPE_BULLET_PLAYER,
  TYPE_BULLET_ENEMY1,
  TYPE_BULLET_ENEMY2,
  TYPE_WATER,
  TYPE_PLATFORM,
  TYPE_HP_BAR,
  TYPE_VISUAL_DOT
} body_type_t;

typedef enum { TURN_PLAYER, TURN_ENEMY1, TURN_ENEMY2 } turn_order_t;

typedef struct {
  body_type_t type;
  bool affected_by_gravity;
  bool is_knocked_back;
  int id;
  double current_hp;
  double max_hp;
  bool died_from_water;
} character_info_t;

typedef enum {
  WAITING_FOR_PLAYER_SHOT,
  PLAYER_SHOT_ACTIVE,
  ENEMY1_FIRING,
  ENEMY1_SHOT_ACTIVE,
  ENEMY2_FIRING,
  ENEMY2_SHOT_ACTIVE,
  SHOT_DELAY,
  GAME_OVER_WATER,
  GAME_OVER_PLAYER_DEAD,
  GAME_WON_PLAYER_WON
} game_status_t;

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
  turn_order_t current_turn;
  double shot_delay_timer;
  turn_order_t next_turn_after_delay;
  bool game_over_message_printed;
  Mix_Music *background_music;
  Mix_Music *low_hp_music;
  bool is_low_hp_music_playing;
  bool audio_initialized;

  bool mouse_down;
  int mouse_x;
  int mouse_y;
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
void update_and_draw_hp_bars(state_t *state);
void check_game_over(state_t *state);
void init_audio_system(state_t *state);
void cleanup_audio_system(state_t *state);
void check_and_update_music(state_t *state);
bool is_any_character_low_hp(state_t *state);
bool is_character_alive(body_t *character);

// initialize audio
void init_audio_system(state_t *state) {
  state->audio_initialized = false;
  state->background_music = NULL;
  state->low_hp_music = NULL;
  state->is_low_hp_music_playing = false;

  // SDL mixer
  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    return;
  }

  // Background music
  state->background_music = Mix_LoadMUS(BACKGROUND_MUSIC_PATH);

  // Load low HP music
  state->low_hp_music = Mix_LoadMUS(LOW_HP_MUSIC_PATH);

  state->audio_initialized = true;

  if (state->background_music) {
    if (Mix_PlayMusic(state->background_music, -1) == -1) {
      printf("Music Failed");
    } else {
      printf("Background music.\n");
    }
  }
}

// Free audio resources
void cleanup_audio_system(state_t *state) {
  if (!state->audio_initialized)
    return;

  // Stop all music
  Mix_HaltMusic();

  // Free music resources
  if (state->background_music) {
    Mix_FreeMusic(state->background_music);
    state->background_music = NULL;
  }

  if (state->low_hp_music) {
    Mix_FreeMusic(state->low_hp_music);
    state->low_hp_music = NULL;
  }

  // Close SDL_mixer
  Mix_CloseAudio();
  state->audio_initialized = false;
}

// check for low hp
bool is_any_character_low_hp(state_t *state) {
  character_info_t *player_info =
      (character_info_t *)body_get_info(state->player);
  character_info_t *enemy1_info =
      (character_info_t *)body_get_info(state->enemy1);
  character_info_t *enemy2_info =
      (character_info_t *)body_get_info(state->enemy2);

  bool player_alive = is_character_alive(state->player);
  bool enemy1_alive = is_character_alive(state->enemy1);
  bool enemy2_alive = is_character_alive(state->enemy2);

  // Check if any alive character has low HP
  if (player_alive && player_info &&
      player_info->current_hp <= LOW_HP_THRESHOLD) {
    return true;
  }
  if (enemy1_alive && enemy1_info &&
      enemy1_info->current_hp <= LOW_HP_THRESHOLD) {
    return true;
  }
  if (enemy2_alive && enemy2_info &&
      enemy2_info->current_hp <= LOW_HP_THRESHOLD) {
    return true;
  }
  return false;
}

// Update music
void check_and_update_music(state_t *state) {
  bool should_play_low_hp = is_any_character_low_hp(state);
  // If low hp & music not playing
  if (should_play_low_hp && !state->is_low_hp_music_playing) {
    if (state->low_hp_music) {
      Mix_HaltMusic();
      if (Mix_PlayMusic(state->low_hp_music, -1) == -1) {
        printf("Music Failed");
      } else {
        printf("Low HP music.\n");
        state->is_low_hp_music_playing = true;
      }
    }
  }
}

// check if alive hp
bool is_character_alive(body_t *character) {
  if (!character || body_is_removed(character)) {
    return false;
  }

  character_info_t *info = (character_info_t *)body_get_info(character);
  if (!info || info->current_hp <= 0 || info->died_from_water) {
    return false;
  }

  return true;
}

// check for game conditions
void check_game_over(state_t *state) {
  if (state->current_status == GAME_OVER_WATER ||
      state->current_status == GAME_OVER_PLAYER_DEAD ||
      state->current_status == GAME_WON_PLAYER_WON) {
    return;
  }

  bool player_alive = is_character_alive(state->player);
  bool enemy1_alive = is_character_alive(state->enemy1);
  bool enemy2_alive = is_character_alive(state->enemy2);

  // Check if player lost
  if (!player_alive) {
    state->current_status = GAME_OVER_PLAYER_DEAD;
    state->game_over_message_printed = false;

    // Stop movement
    body_set_velocity(state->player, VEC_ZERO);
    body_set_velocity(state->enemy1, VEC_ZERO);
    body_set_velocity(state->enemy2, VEC_ZERO);

    character_info_t *player_info =
        (character_info_t *)body_get_info(state->player);
    character_info_t *enemy1_info =
        (character_info_t *)body_get_info(state->enemy1);
    character_info_t *enemy2_info =
        (character_info_t *)body_get_info(state->enemy2);

    if (player_info) {
      player_info->affected_by_gravity = false;
      player_info->is_knocked_back = false;
    }
    if (enemy1_info) {
      enemy1_info->affected_by_gravity = false;
      enemy1_info->is_knocked_back = false;
    }
    if (enemy2_info) {
      enemy2_info->affected_by_gravity = false;
      enemy2_info->is_knocked_back = false;
    }
  }
  // Check if player won
  else if (player_alive && !enemy1_alive && !enemy2_alive) {
    state->current_status = GAME_WON_PLAYER_WON;
    state->game_over_message_printed = false;

    // Stop movement
    body_set_velocity(state->player, VEC_ZERO);
    body_set_velocity(state->enemy1, VEC_ZERO);
    body_set_velocity(state->enemy2, VEC_ZERO);

    character_info_t *player_info =
        (character_info_t *)body_get_info(state->player);
    character_info_t *enemy1_info =
        (character_info_t *)body_get_info(state->enemy1);
    character_info_t *enemy2_info =
        (character_info_t *)body_get_info(state->enemy2);

    if (player_info) {
      player_info->affected_by_gravity = false;
      player_info->is_knocked_back = false;
    }
    if (enemy1_info) {
      enemy1_info->affected_by_gravity = false;
      enemy1_info->is_knocked_back = false;
    }
    if (enemy2_info) {
      enemy2_info->affected_by_gravity = false;
      enemy2_info->is_knocked_back = false;
    }
  }
}

// calculates turn order player -> enemy
turn_order_t get_next_turn(turn_order_t last_turn, body_t *player,
                           body_t *enemy1, body_t *enemy2) {
  bool player_alive = is_character_alive(player);
  bool enemy1_alive = is_character_alive(enemy1);
  bool enemy2_alive = is_character_alive(enemy2);

  // cycle through, skipping dead characters
  switch (last_turn) {
  case TURN_PLAYER:
    if (enemy1_alive)
      return TURN_ENEMY1;
    else if (enemy2_alive)
      return TURN_ENEMY2;
    else if (player_alive)
      return TURN_PLAYER;
    break;
  case TURN_ENEMY1:
    if (enemy2_alive)
      return TURN_ENEMY2;
    else if (player_alive)
      return TURN_PLAYER;
    else if (enemy1_alive)
      return TURN_ENEMY1;
    break;
  case TURN_ENEMY2:
    if (player_alive)
      return TURN_PLAYER;
    else if (enemy1_alive)
      return TURN_ENEMY1;
    else if (enemy2_alive)
      return TURN_ENEMY2;
    break;
  }

  return TURN_PLAYER;
}

body_t *make_visual_dots(vector_t center, double radius) {
  list_t *c = list_init(CIRC_NPOINTS, free);

  for (size_t i = 0; i < CIRC_NPOINTS; i++) {
    double angle = 2 * M_PI * i / CIRC_NPOINTS;

    vector_t *v = malloc(sizeof(*v));
    assert(v);

    vector_t unit = {cos(angle), sin(angle)};
    *v = vec_add(vec_multiply(radius, unit), center);

    list_add(c, v);
  }

  character_info_t *info = malloc(sizeof(character_info_t));
  info->type = TYPE_VISUAL_DOT;
  return body_init_with_info(c, 10, WHITE, info, free);
}

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
  info->died_from_water = false;

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
  vector_t bullet_velocity = {dir_x * BULLET_VELOCITY, 0};

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
  if (type == KEY_PRESSED && current_state->current_status != GAME_OVER_WATER &&
      current_state->current_status != GAME_OVER_PLAYER_DEAD) {
    if (key == SPACE_BAR &&
        current_state->current_status == WAITING_FOR_PLAYER_SHOT &&
        current_state->current_turn == TURN_PLAYER) {
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
  if (current_state->current_status == GAME_OVER_WATER ||
      current_state->current_status == GAME_OVER_PLAYER_DEAD) {
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

  target_char_info->affected_by_gravity = true;
  target_char_info->is_knocked_back = true;
  vector_t bullet_vel = body_get_velocity(bullet);
  double knock_dir_x = (bullet_vel.x > 0) ? 1.0 : -1.0;
  body_set_velocity(target, (vector_t){knock_dir_x * KNOCKBACK_BASE_VELOCITY_X,
                                       KNOCKBACK_BASE_VELOCITY_Y});

  check_game_over(current_state);

  // if game not over
  if (current_state->current_status != GAME_OVER_PLAYER_DEAD &&
      current_state->current_status != GAME_OVER_WATER) {
    current_state->next_turn_after_delay =
        get_next_turn(current_state->current_turn, current_state->player,
                      current_state->enemy1, current_state->enemy2);

    current_state->current_status = SHOT_DELAY;
    current_state->shot_delay_timer = SHOT_DELAY_TIME;
    printf("Starting 1-second delay before next turn...\n");
  }

  body_remove(bullet);
}

void character_hit_water_handler(body_t *character, body_t *water,
                                 vector_t axis, void *aux, double force_const) {
  state_t *current_state = (state_t *)aux;
  character_info_t *char_info = (character_info_t *)body_get_info(character);

  if (!char_info || current_state->current_status == GAME_OVER_WATER ||
      current_state->current_status == GAME_OVER_PLAYER_DEAD)
    return;

  color_t current_color = body_get_color(character);
  if (fabs(current_color.red - WATER_DEATH_COLOR.red) > 0.01 ||
      fabs(current_color.green - WATER_DEATH_COLOR.green) > 0.01 ||
      fabs(current_color.blue - WATER_DEATH_COLOR.blue) > 0.01) {
    printf("Character ID %d touched water.\n", char_info->id);
    body_set_color(character, WATER_DEATH_COLOR);
    body_set_velocity(character, VEC_ZERO);
    char_info->died_from_water = true;
    char_info->affected_by_gravity = false;
    char_info->is_knocked_back = false;
    current_state->current_status = GAME_OVER_WATER;
    current_state->game_over_message_printed = false;
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
  if (current_state->current_status == GAME_OVER_WATER ||
      current_state->current_status == GAME_OVER_PLAYER_DEAD)
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

void mouse_handler(mouse_event_type_t type, int x, int y, state_t *state) {
  switch (type) {
  case MOUSE_DOWN: {
    state->mouse_down = true;
    state->mouse_x = 0;
    state->mouse_y = 0;
    printf("Mouse down!\n");
    break;
  }
  case MOUSE_MOVE: {
    if (state->mouse_down) {
      state->mouse_x = x;
      state->mouse_y = y;
    }
    break;
  }
  case MOUSE_UP: {
    state->mouse_down = false;
    printf("Mouse up!\n");
    break;
  }
  default:
    break;
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
  current_state->current_turn = TURN_PLAYER;
  current_state->shot_delay_timer = 0.0;
  current_state->next_turn_after_delay = TURN_PLAYER;
  current_state->game_over_message_printed = false;
  current_state->all_characters = list_init(3, NULL);

  // Initialize audio system
  init_audio_system(current_state);

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
  asset_make_image_with_body(PLAYER1_PATH, current_state->player);
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
  asset_make_image_with_body(ENEMY_PATH, current_state->enemy1);
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
  asset_make_image_with_body(ENEMY_PATH, current_state->enemy2);
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

  current_state->mouse_down = false;
  current_state->mouse_x = 0;
  current_state->mouse_y = 0;

  sdl_on_key((key_handler_t)on_key_press);
  sdl_on_mouse((mouse_handler_t)mouse_handler);

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

void update_and_draw_visualization(state_t *state) {
  // unit vector for aiming direction
  const vector_t mouse = (vector_t){
      (double)state->mouse_x, MAX_SCREEN_COORDS.y - (double)state->mouse_y};
  vector_t player_center = body_get_centroid(state->player);
  vector_t diff = vec_subtract(player_center, mouse);

  if (diff.x < 0 || diff.y < 0) {
    return;
  }

  diff.x *= BULLET_VELOCITY / player_center.x * MOUSE_SCALE;
  diff.y *= BULLET_VELOCITY / player_center.y * MOUSE_SCALE;

  // Constrain distance
  if (diff.x > BULLET_VELOCITY)
    diff.x = BULLET_VELOCITY;
  if (diff.y > BULLET_VELOCITY)
    diff.y = BULLET_VELOCITY;

  for (size_t i = 1; i <= N_DOTS; i++) {
    body_t *dot =
        make_visual_dots(body_get_centroid(state->player), DOT_RADIUS);
    body_set_velocity(dot, diff);
    body_add_force(dot, (vector_t){0, -GRAVITY_ACCELERATION * BULLET_WEIGHT});
    body_tick(dot, DOTS_SEP_DT * i);
    sdl_draw_body(dot);
    /* printf("centroid %zu: %f, mouse x = %f, mouse y = %f, diff x = %f, y =
     * %f\n", i, body_get_centroid(dot).x, mouse.x, mouse.y, diff.x, diff.y); */
  }
}

bool emscripten_main(state_t *current_state) {
  double dt = time_since_last_tick();

  // check if game over
  check_game_over(current_state);

  // update music
  check_and_update_music(current_state);

  // update visualization
  /* track_cursor(current_state); */

  if (current_state->current_status != GAME_OVER_WATER &&
      current_state->current_status != GAME_OVER_PLAYER_DEAD &&
      current_state->current_status != GAME_WON_PLAYER_WON) {
    // delay between shots
    if (current_state->current_status == SHOT_DELAY) {
      current_state->shot_delay_timer -= dt;

      if (current_state->shot_delay_timer <= 0.0) {
        current_state->current_turn = current_state->next_turn_after_delay;

        if (current_state->current_turn == TURN_PLAYER) {
          current_state->current_status = WAITING_FOR_PLAYER_SHOT;
          printf("Player's turn to shoot!\n");
        } else if (current_state->current_turn == TURN_ENEMY1) {
          current_state->current_status = ENEMY1_FIRING;
          printf("Enemy 1's turn to shoot!\n");
        } else if (current_state->current_turn == TURN_ENEMY2) {
          current_state->current_status = ENEMY2_FIRING;
          printf("Enemy 2's turn to shoot!\n");
        }
      }
    }

    if (current_state->current_turn == TURN_ENEMY1 &&
        current_state->current_status == ENEMY1_FIRING) {
      printf("Enemy 1 firing.\n");
      fire_bullet(current_state, current_state->enemy1, current_state->player,
                  TYPE_BULLET_ENEMY1);
      current_state->current_status = ENEMY1_SHOT_ACTIVE;
    } else if (current_state->current_turn == TURN_ENEMY2 &&
               current_state->current_status == ENEMY2_FIRING) {
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
        if (info && (*info == TYPE_PLAYER || *info == TYPE_ENEMY)) {
          continue;
        }
        sdl_draw_body(body_to_draw);
      }
    }
  }

  // This will now create temporary bodies for HP bars and draw them
  update_and_draw_hp_bars(current_state);

  // Visualization
  const bool easy = true;
  if (easy && current_state->current_status == WAITING_FOR_PLAYER_SHOT &&
      current_state->mouse_down)
    update_and_draw_visualization(current_state);

  // Display game over message
  if ((current_state->current_status == GAME_OVER_PLAYER_DEAD ||
       current_state->current_status == GAME_OVER_WATER ||
       current_state->current_status == GAME_WON_PLAYER_WON) &&
      !current_state->game_over_message_printed) {
    if (current_state->current_status == GAME_OVER_PLAYER_DEAD) {
      printf("GAME OVER - Player Lost!\n");
    } else if (current_state->current_status == GAME_OVER_WATER) {
      printf("GAME OVER - Player fell in the water!\n");
    } else if (current_state->current_status == GAME_WON_PLAYER_WON) {
      printf("VICTORY - Player Won! New Pirate King!\n");
    }
    current_state->game_over_message_printed = true;
  }

  sdl_show();

  return false;
}

void emscripten_free(state_t *current_state) {
  cleanup_audio_system(current_state);
  list_free(current_state->all_characters);
  scene_free(current_state->scene);
  asset_cache_destroy();
  free(current_state);
}
