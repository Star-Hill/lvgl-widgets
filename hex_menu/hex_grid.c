#include "hex_grid.h"
#include <math.h>

#define HEX_SQRT3 1.7320508075688772f

void hex_axial_to_px(hex_axial_t a, float s, float * out_x, float * out_y)
{
    *out_x = s * HEX_SQRT3 * ((float)a.q + (float)a.r * 0.5f);
    *out_y = s * 1.5f * (float)a.r;
}

hex_axial_t hex_px_to_axial(float x, float y, float s)
{
    hex_axial_t out;

    /* hex_axial_to_px 的逆，得到浮点轴向坐标 */
    float rf = y / (s * 1.5f);
    float qf = x / (s * HEX_SQRT3) - rf * 0.5f;

    /* 立方坐标取整：三维分别四舍五入后，把舍入误差最大的一维用另两维反推，
     * 以维持 x + y + z == 0 的约束。直接对 q、r 各自取整会取到错误的格子。 */
    float cx = qf;
    float cz = rf;
    float cy = -cx - cz;

    float rx = roundf(cx);
    float ry = roundf(cy);
    float rz = roundf(cz);

    float dx = fabsf(rx - cx);
    float dy = fabsf(ry - cy);
    float dz = fabsf(rz - cz);

    if(dx > dy && dx > dz)  rx = -ry - rz;
    else if(dy > dz)        ry = -rx - rz;
    else                    rz = -rx - ry;

    (void)ry;   /* ry 只参与上面的修正，不进入返回值 */

    out.q = (int)rx;
    out.r = (int)rz;
    return out;
}

int hex_slot_index(int q, int r)
{
    /* 系数 27 由「两个晶格向量都映射到 0」解出：
     *   4x + 3y ≡ 0 (mod 37)，-3x + 7y ≡ 0 (mod 37)
     *   4⁻¹ mod 37 = 28  =>  x ≡ -3y·28 ≡ -84y ≡ 27y，取 y = 1 得 x = 27
     * 代入第二式验证：-81 + 7 = -74 = -2·37 ≡ 0 ✓ */
    int v = (27 * q + r) % HEX_SLOT_CNT;
    return v < 0 ? v + HEX_SLOT_CNT : v;
}
