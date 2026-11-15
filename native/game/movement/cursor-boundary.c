/* Authored battlefield edges can cross hills. Retail 112C00 usually limits
 * the flat-plane cursor, but movement commands consume the ground ray at
 * 801F0DC8. Match the cursor-control queries and final ground query (ROM
 * 1038AC), using the game's own height and projection functions.
 * Original maps and the separately scripted Fort Romulus retain their path. */
typedef unsigned char u8;
extern const u8 lc_cursor_boundary_keys[256];
extern u8 lc_cursor_key;
extern u8 lc_cursor_selector;
extern float lc_cursor_bounds[4];
extern float lc_cursor_flat[3];
extern void *lc_cursor_object;
extern int lc_cursor_ray(int, void *, float, float, float *);
extern float lc_cursor_height(int, float, float);
extern void lc_cursor_project(void *, const float *, float *, float *);
extern void lc_cursor_plane(void *, float, float, float *);

int lc_cursor_ground(int selector, void *camera, float x, float y, float *point)
{
    int result = lc_cursor_ray(selector, camera, x, y, point);
    float px = point[0], pz = point[2];
    float *screen;
    if (!lc_cursor_boundary_keys[lc_cursor_key]) return result;
    if (px < lc_cursor_bounds[0]) px = lc_cursor_bounds[0];
    if (px > lc_cursor_bounds[2]) px = lc_cursor_bounds[2];
    if (pz < lc_cursor_bounds[1]) pz = lc_cursor_bounds[1];
    if (pz > lc_cursor_bounds[3]) pz = lc_cursor_bounds[3];
    if (px == point[0] && pz == point[2]) return result;
    point[0] = px;
    point[2] = pz;
    point[1] = lc_cursor_height(selector, px, pz);
    screen = (float *)((u8 *)lc_cursor_object + 8);
    lc_cursor_project(camera, point, &screen[0], &screen[1]);
    lc_cursor_plane(camera, screen[0], screen[1], lc_cursor_flat);
    return result;
}

/* The two cursor-control consumers normally use the flat plane on outdoor
 * maps. An edited edge through a hill must use the same ground point as the
 * eventual order, including when moving the camera at the screen boundary. */
void lc_cursor_plane_checked(void *camera, float x, float y, float *point)
{
    if (lc_cursor_boundary_keys[lc_cursor_key])
        lc_cursor_ground(lc_cursor_selector, camera, x, y, point);
    else
        lc_cursor_plane(camera, x, y, point);
}
