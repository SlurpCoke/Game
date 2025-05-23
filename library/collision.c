#include "collision.h"
#include "body.h"
#include "list.h"
#include "vector.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>

/**
 * Returns a list of vectors representing the edges of a shape.
 *
 * @param shape the list of vectors representing the vertices of a shape
 * @return a list of vectors representing the edges of the shape
 */
static list_t *get_edges(list_t *shape) {
  list_t *edges = list_init(list_size(shape), free);

  for (size_t i = 0; i < list_size(shape); i++) {
    vector_t *vec = malloc(sizeof(vector_t));
    assert(vec);
    *vec =
        vec_subtract(*(vector_t *)list_get(shape, i % list_size(shape)),
                     *(vector_t *)list_get(shape, (i + 1) % list_size(shape)));
    list_add(edges, vec);
  }

  return edges;
}

/**
 * Returns a vector containing the maximum and minimum length projections given
 * a unit axis and shape.
 *
 * @param shape the list of vectors representing the vertices of a shape
 * @param unit_axis the unit axis to project eeach vertex on
 * @return a vector in the form (max, min) where `max` is the maximum projection
 * length and `min` is the minimum projection length.
 */
static vector_t get_max_min_projections(list_t *shape, vector_t unit_axis) {
  double max_projection = -DBL_MAX;
  double min_projection = DBL_MAX;

  for (size_t i = 0; i < list_size(shape); i++) {
    vector_t *vertex = (vector_t *)list_get(shape, i);
    double projection = vec_dot(*vertex, unit_axis);

    if (projection > max_projection) {
      max_projection = projection;
    }
    if (projection < min_projection) {
      min_projection = projection;
    }
  }

  return (vector_t){max_projection, min_projection};
}

static vector_t centroid_from_vertices(list_t *vertices) {
  vector_t sum_v = VEC_ZERO;
  size_t num_vertices = list_size(vertices);
  
  for (size_t i = 0; i < num_vertices; i++) {
    sum_v = vec_add(sum_v, *(vector_t *)list_get(vertices, i));
  }
  return vec_multiply(1.0 / num_vertices, sum_v);
}

/**
 * Determines whether two convex polygons intersect.
 * The polygons are given as lists of vertices in counterclockwise order.
 * There is an edge between each pair of consecutive vertices,
 * and one between the first vertex and the last vertex.
 *
 * @param shape1 the first shape
 * @param shape2 the second shape
 * @return whether the shapes are colliding
 */
static collision_info_t compare_collision(list_t *shape1, list_t *shape2, double *min_overlap) {
  collision_info_t result_info;
  result_info.collided = false;
  result_info.axis = VEC_ZERO;

  double current_min_overlap = __DBL_MAX__;
  vector_t axis_min_overlap = VEC_ZERO;
  
  list_t *edges1 = get_edges(shape1);
  assert(edges1 != NULL);

  for (size_t i = 0; i < list_size(edges1); i++) {
    vector_t edge = *(vector_t *)list_get(edges1, i);

    vector_t axis = {-edge.y, edge.x};

    double length = vec_get_length(axis);
    if (length < 1e-9) {
      continue;
    }
    vector_t unit_axis = vec_multiply(1.0 / length, axis);

    vector_t projections1 = get_max_min_projections(shape1, unit_axis);
    vector_t projections2 = get_max_min_projections(shape2, unit_axis);

    if (projections1.x <= projections2.y || projections2.x <= projections1.y) {
      list_free(edges1);
      result_info.collided = false;
      return result_info;
    }

    double overlap = fmin(projections1.x, projections2.x) - fmax(projections1.y, projections2.y);

    if (overlap < current_min_overlap) {
      current_min_overlap = overlap;
      axis_min_overlap = unit_axis;
    }
  }

  list_free(edges1);

  result_info.collided = true;
  *min_overlap = current_min_overlap;

  vector_t centroid1 = centroid_from_vertices(shape1);
  vector_t centroid2 = centroid_from_vertices(shape2);
  vector_t c1_to_c2 = vec_subtract(centroid2, centroid1);

  if (vec_dot(axis_min_overlap, c1_to_c2) < 0) {
    axis_min_overlap = vec_negate(axis_min_overlap);
  }
  result_info.axis = axis_min_overlap;

  return result_info;
}

collision_info_t find_collision(body_t *body1, body_t *body2) {
  list_t *shape1 = body_get_shape(body1);
  list_t *shape2 = body_get_shape(body2);

  double c1_overlap = __DBL_MAX__;
  double c2_overlap = __DBL_MAX__;

  collision_info_t collision1 = compare_collision(shape1, shape2, &c1_overlap);
  collision_info_t collision2 = compare_collision(shape2, shape1, &c2_overlap);

  list_free(shape1);
  list_free(shape2);

  if (!collision1.collided) {
    return collision1;
  }

  if (!collision2.collided) {
    return collision2;
  }

  if (c1_overlap < c2_overlap) {
    return collision1;
  }
  return collision2;
}
