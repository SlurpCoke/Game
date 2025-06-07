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