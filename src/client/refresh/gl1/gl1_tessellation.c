#include "header/local.h"

/* Control points for a single patch */
typedef struct {
    vec3_t b300, b030, b003;  // corner control points
    vec3_t b210, b120, b021, b012, b102, b201;  // edge control points
    vec3_t b111;  // center control point
} pn_patch_t;

/* Tessellation parameters */
static cvar_t *gl1_tessellation = NULL;
static cvar_t *gl1_tessellation_level = NULL;

/*
 * Calculate control points for PN triangles patch 
 */
static void 
R_ComputePNPatch(const vec3_t v0, const vec3_t v1, const vec3_t v2, 
          const vec3_t n0, const vec3_t n1, const vec3_t n2,
          pn_patch_t *patch)
{
    int i;
    vec3_t w12, w21, w03, w30, w20, w02;
    float v12, v21, v03, v30, v20, v02;

    // Corner control points (P300, P030, P003)
    VectorCopy(v0, patch->b300);
    VectorCopy(v1, patch->b030);
    VectorCopy(v2, patch->b003);

    // Edge control points
    for (i = 0; i < 3; i++) {
        // P210
        patch->b210[i] = (2.0f * v0[i] + v1[i]) / 3.0f;
        // P120 
        patch->b120[i] = (2.0f * v1[i] + v0[i]) / 3.0f;
        // P021
        patch->b021[i] = (2.0f * v1[i] + v2[i]) / 3.0f;
  // P012
        patch->b012[i] = (2.0f * v2[i] + v1[i]) / 3.0f;
   // P102
      patch->b102[i] = (2.0f * v2[i] + v0[i]) / 3.0f;
        // P201
  patch->b201[i] = (2.0f * v0[i] + v2[i]) / 3.0f;
    }

    // Calculate tangent vectors
    VectorSubtract(patch->b210, patch->b300, w12);
    VectorSubtract(patch->b120, patch->b030, w21);
    VectorSubtract(patch->b021, patch->b030, w03);
    VectorSubtract(patch->b012, patch->b003, w30);
    VectorSubtract(patch->b102, patch->b003, w20);
    VectorSubtract(patch->b201, patch->b300, w02);

    // Project normals
    v12 = DotProduct(n0, w12);
    v21 = DotProduct(n1, w21); 
    v03 = DotProduct(n1, w03);
    v30 = DotProduct(n2, w30);
    v20 = DotProduct(n2, w20);
    v02 = DotProduct(n0, w02);

    // Adjust edge control points
    VectorMA(patch->b210, v12, n0, patch->b210);
    VectorMA(patch->b120, v21, n1, patch->b120);
 VectorMA(patch->b021, v03, n1, patch->b021);
    VectorMA(patch->b012, v30, n2, patch->b012);
    VectorMA(patch->b102, v20, n2, patch->b102);
    VectorMA(patch->b201, v02, n0, patch->b201);

    // Center control point (P111)
    for (i = 0; i < 3; i++) {
     patch->b111[i] = (patch->b300[i] + patch->b030[i] + patch->b003[i] +
           patch->b210[i] + patch->b120[i] + patch->b021[i] +
  patch->b012[i] + patch->b102[i] + patch->b201[i]) / 9.0f;
    }
}

/*
 * Evaluate a point on the PN triangles patch
 */
static void
R_EvaluatePNPatch(const pn_patch_t *patch, float u, float v, vec3_t point)
{
    float w = 1.0f - u - v;
    float u2 = u * u;
    float v2 = v * v;
    float w2 = w * w;
    float u3 = u2 * u;
    float v3 = v2 * v;
    float w3 = w2 * w;

    VectorScale(patch->b300, w3, point);
    VectorMA(point, u3, patch->b030, point);
    VectorMA(point, v3, patch->b003, point);
    VectorMA(point, 3.0f * w2 * u, patch->b210, point);
    VectorMA(point, 3.0f * w * u2, patch->b120, point);
    VectorMA(point, 3.0f * w2 * v, patch->b201, point);
    VectorMA(point, 3.0f * u2 * v, patch->b021, point);
    VectorMA(point, 3.0f * w * v2, patch->b102, point);
  VectorMA(point, 3.0f * u * v2, patch->b012, point);
    VectorMA(point, 6.0f * w * u * v, patch->b111, point);
}

/*
 * Initialize tessellation CVars and parameters
 */
void 
R_InitTessellation(void)
{
    gl1_tessellation = ri.Cvar_Get("gl1_tessellation", "1", CVAR_ARCHIVE);
    gl1_tessellation_level = ri.Cvar_Get("gl1_tessellation_level", "4", CVAR_ARCHIVE);
}

/*
 * Tessellate a triangle using PN triangles
 * Returns number of vertices generated
 */
int
R_TessellateTriangle(const vec3_t v0, const vec3_t v1, const vec3_t v2,
    const vec3_t n0, const vec3_t n1, const vec3_t n2,
          vec3_t *out_vertices, vec3_t *out_normals)
{
    int i, j, num_vertices;
    float step, u, v;
    pn_patch_t patch;
vec3_t e1, e2, normal;

    if (!gl1_tessellation->value) {
        // Pass through original triangle if tessellation disabled
 VectorCopy(v0, out_vertices[0]);
      VectorCopy(v1, out_vertices[1]);  
        VectorCopy(v2, out_vertices[2]);
        if (out_normals) {
   VectorCopy(n0, out_normals[0]);
      VectorCopy(n1, out_normals[1]);
        VectorCopy(n2, out_normals[2]);
        }
  return 3;
    }

    // Compute PN triangles control points
    R_ComputePNPatch(v0, v1, v2, n0, n1, n2, &patch);

    // Tessellate based on tessellation level
  step = 1.0f / gl1_tessellation_level->value;
    num_vertices = 0;

    for (i = 0; i <= gl1_tessellation_level->value; i++) {
        u = i * step;
        for (j = 0; j <= gl1_tessellation_level->value - i; j++) {
        v = j * step;

            // Evaluate surface point
   R_EvaluatePNPatch(&patch, u, v, out_vertices[num_vertices]);

   if (out_normals) {
          // Calculate normal by central differences
     vec3_t du, dv;
           float eps = 0.001f;

        R_EvaluatePNPatch(&patch, u + eps, v, du);
          R_EvaluatePNPatch(&patch, u, v + eps, dv);
 VectorSubtract(du, out_vertices[num_vertices], e1);
       VectorSubtract(dv, out_vertices[num_vertices], e2);
 CrossProduct(e1, e2, normal);
       VectorNormalize(normal);
        VectorCopy(normal, out_normals[num_vertices]);
          }

         num_vertices++;
    }
    }

    return num_vertices;
}