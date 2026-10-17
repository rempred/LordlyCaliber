/* Includes the entry preamble at 28850 before any output is stored. */
void mtx_apply_vec3(float m[4][4], float x, float y, float z, float *ox, float *oy, float *oz)
{
    *ox = m[0][0]*x + m[1][0]*y + m[2][0]*z + m[3][0];
    *oy = m[0][1]*x + m[1][1]*y + m[2][1]*z + m[3][1];
    *oz = m[0][2]*x + m[1][2]*y + m[2][2]*z + m[3][2];
}
