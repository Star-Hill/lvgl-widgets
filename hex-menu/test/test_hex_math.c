#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "hex_grid.h"
#include "hex_physics.h"

static int g_fail = 0;

#define CHECK(cond, ...)                                              \
    do {                                                              \
        if(!(cond)) {                                                 \
            printf("FAIL %s:%d  ", __FILE__, __LINE__);               \
            printf(__VA_ARGS__);                                      \
            printf("\n");                                             \
            g_fail++;                                                 \
        }                                                             \
    } while(0)

/* 晶格向量：L1 = (a, b) = (4, 3)，L2 = (-b, a+b) = (-3, 7) */
#define L1Q  4
#define L1R  3
#define L2Q (-3)
#define L2R  7

static void test_slot_lattice_vectors_map_to_zero(void)
{
    CHECK(hex_slot_index(0, 0) == 0, "origin -> %d", hex_slot_index(0, 0));
    CHECK(hex_slot_index(L1Q, L1R) == 0, "L1 -> %d", hex_slot_index(L1Q, L1R));
    CHECK(hex_slot_index(L2Q, L2R) == 0, "L2 -> %d", hex_slot_index(L2Q, L2R));
}

static void test_slot_range_and_surjectivity(void)
{
    int seen[HEX_SLOT_CNT];
    int distinct = 0;
    for(int i = 0; i < HEX_SLOT_CNT; i++) seen[i] = 0;

    for(int q = -60; q <= 60; q++) {
        for(int r = -60; r <= 60; r++) {
            int s = hex_slot_index(q, r);
            CHECK(s >= 0 && s < HEX_SLOT_CNT, "slot %d out of range at (%d,%d)", s, q, r);
            if(s >= 0 && s < HEX_SLOT_CNT && seen[s]++ == 0) distinct++;
        }
    }
    CHECK(distinct == HEX_SLOT_CNT, "distinct=%d expected %d", distinct, HEX_SLOT_CNT);
}

static void test_slot_lattice_translation_invariant(void)
{
    srand(1);
    for(int i = 0; i < 200000; i++) {
        int q = rand() % 199 - 99;
        int r = rand() % 199 - 99;
        int m = rand() % 19 - 9;
        int n = rand() % 19 - 9;
        int a = hex_slot_index(q, r);
        int b = hex_slot_index(q + m * L1Q + n * L2Q, r + m * L1R + n * L2R);
        CHECK(a == b, "(%d,%d) m=%d n=%d : %d != %d", q, r, m, n, a, b);
        if(a != b) return;   /* 一条就够，不刷屏 */
    }
}

static void test_radius3_hexagon_is_fundamental_domain(void)
{
    int seen[HEX_SLOT_CNT];
    int cells = 0;
    for(int i = 0; i < HEX_SLOT_CNT; i++) seen[i] = 0;

    for(int q = -3; q <= 3; q++) {
        for(int r = -3; r <= 3; r++) {
            if(abs(q + r) > 3) continue;      /* 轴向坐标下半径 3 的六边形 */
            cells++;
            int s = hex_slot_index(q, r);
            CHECK(seen[s]++ == 0, "duplicate slot %d at (%d,%d)", s, q, r);
        }
    }
    CHECK(cells == HEX_SLOT_CNT, "radius-3 hexagon has %d cells, expected %d", cells, HEX_SLOT_CNT);
}

static void test_px_axial_roundtrip_on_centers(void)
{
    const float s = 80.0f;
    for(int q = -20; q <= 20; q++) {
        for(int r = -20; r <= 20; r++) {
            hex_axial_t in = { q, r };
            float x, y;
            hex_axial_to_px(in, s, &x, &y);
            hex_axial_t out = hex_px_to_axial(x, y, s);
            CHECK(out.q == q && out.r == r,
                  "roundtrip (%d,%d) -> (%.1f,%.1f) -> (%d,%d)", q, r, x, y, out.q, out.r);
            if(out.q != q || out.r != r) return;
        }
    }
}

static void test_px_to_axial_returns_nearest_center(void)
{
    const float s = 80.0f;
    srand(7);
    for(int i = 0; i < 20000; i++) {
        float px = (float)(rand() % 4000 - 2000) * 0.5f;
        float py = (float)(rand() % 4000 - 2000) * 0.5f;

        hex_axial_t got = hex_px_to_axial(px, py, s);
        float gx, gy;
        hex_axial_to_px(got, s, &gx, &gy);
        float best = (px - gx) * (px - gx) + (py - gy) * (py - gy);

        /* 在返回格子周围 2 圈内暴力搜索，不应存在更近的格心 */
        for(int dq = -2; dq <= 2; dq++) {
            for(int dr = -2; dr <= 2; dr++) {
                hex_axial_t cand = { got.q + dq, got.r + dr };
                float cx, cy;
                hex_axial_to_px(cand, s, &cx, &cy);
                float d = (px - cx) * (px - cx) + (py - cy) * (py - cy);
                CHECK(d >= best - 0.01f, "(%.2f,%.2f): (%d,%d) d=%.2f closer than (%d,%d) d=%.2f",
                      px, py, cand.q, cand.r, d, got.q, got.r, best);
                if(d < best - 0.01f) return;
            }
        }
    }
}

static void test_smoothstep_endpoints_and_monotonic(void)
{
    CHECK(hex_smoothstep(-1.0f) == 0.0f, "t<0 -> %f", hex_smoothstep(-1.0f));
    CHECK(hex_smoothstep(0.0f) == 0.0f, "t=0 -> %f", hex_smoothstep(0.0f));
    CHECK(hex_smoothstep(1.0f) == 1.0f, "t=1 -> %f", hex_smoothstep(1.0f));
    CHECK(hex_smoothstep(2.0f) == 1.0f, "t>1 -> %f", hex_smoothstep(2.0f));
    CHECK(fabsf(hex_smoothstep(0.5f) - 0.5f) < 1e-6f, "t=0.5 -> %f", hex_smoothstep(0.5f));

    float prev = -1.0f;
    for(int i = 0; i <= 1000; i++) {
        float t = (float)i / 1000.0f;
        float v = hex_smoothstep(t);
        CHECK(v >= prev, "not monotonic at t=%f: %f < %f", t, v, prev);
        CHECK(v >= 0.0f && v <= 1.0f, "out of [0,1] at t=%f: %f", t, v);
        if(v < prev) return;
        prev = v;
    }
}

static void test_magnifier_t(void)
{
    CHECK(hex_magnifier_t(0.0f, 420.0f) == 0.0f, "d=0 -> %f", hex_magnifier_t(0.0f, 420.0f));
    CHECK(hex_magnifier_t(420.0f, 420.0f) == 1.0f, "d=r -> %f", hex_magnifier_t(420.0f, 420.0f));
    CHECK(hex_magnifier_t(9999.0f, 420.0f) == 1.0f, "d>>r -> %f", hex_magnifier_t(9999.0f, 420.0f));
    CHECK(hex_magnifier_t(100.0f, 0.0f) == 1.0f, "r=0 must not divide by zero");
}

static void test_friction_decays_monotonically_to_zero(void)
{
    float v = 2000.0f;
    float prev = v;
    int frames = 0;
    while(v > 12.0f && frames < 100000) {
        v *= hex_friction_decay(16.0f, 260.0f);
        CHECK(v < prev, "friction not strictly decreasing: %f >= %f", v, prev);
        if(v >= prev) return;
        prev = v;
        frames++;
    }
    CHECK(frames < 100000, "friction never fell below threshold");
    /* tau=260ms 下 2000 -> 12 约需 ln(2000/12)*260/16 ≈ 83 帧 */
    CHECK(frames > 40 && frames < 200, "unexpected settle frames: %d", frames);
}

static void test_spring_converges_without_overshoot(void)
{
    const float omega = 12.0f;
    const float dt = 1.0f / 60.0f;

    srand(11);
    for(int c = 0; c < 500; c++) {
        float x0 = (float)(rand() % 2000 - 1000) * 0.1f;   /* ±100 px */
        if(fabsf(x0) < 1.0f) continue;
        /* 前提：|v0| <= omega*|x0|，由 HEX_V_SNAP 阈值保证 */
        float vmax = omega * fabsf(x0);
        float v0 = ((float)(rand() % 2001 - 1000) / 1000.0f) * vmax;

        float pos = x0, vel = v0;
        float worst_overshoot = 0.0f;
        for(int i = 0; i < 2000; i++) {
            hex_spring_step(&pos, &vel, 0.0f, omega, dt);
            /* 过冲 = 越过目标(0)到达的反向距离 */
            float os = (x0 > 0.0f) ? -pos : pos;
            if(os > worst_overshoot) worst_overshoot = os;
        }
        CHECK(fabsf(pos) < 0.01f, "did not converge: x0=%f v0=%f -> %f", x0, v0, pos);
        CHECK(worst_overshoot <= fabsf(x0) * 0.02f,
              "overshoot %f exceeds 2%% of |x0|=%f", worst_overshoot, fabsf(x0));
        if(fabsf(pos) >= 0.01f || worst_overshoot > fabsf(x0) * 0.02f) return;
    }
}

int main(void)
{
    test_slot_lattice_vectors_map_to_zero();
    test_slot_range_and_surjectivity();
    test_slot_lattice_translation_invariant();
    test_radius3_hexagon_is_fundamental_domain();
    test_px_axial_roundtrip_on_centers();
    test_px_to_axial_returns_nearest_center();

    test_smoothstep_endpoints_and_monotonic();
    test_magnifier_t();
    test_friction_decays_monotonically_to_zero();
    test_spring_converges_without_overshoot();

    if(g_fail == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d CHECK(s) FAILED\n", g_fail);
    return 1;
}
