#include "hex_physics.h"
#include <math.h>

float hex_smoothstep(float t)
{
    if(t <= 0.0f) return 0.0f;
    if(t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

float hex_magnifier_t(float d, float r)
{
    if(r <= 0.0f) return 1.0f;
    return hex_smoothstep(d / r);
}

float hex_friction_decay(float dt_ms, float tau_ms)
{
    if(tau_ms <= 0.0f) return 0.0f;
    return expf(-dt_ms / tau_ms);
}

void hex_spring_step(float * pos, float * vel, float target, float omega, float dt)
{
    /* 临界阻尼：k = ω²，c = 2ω。阻尼比恰为 1，不振荡且收敛最快。 */
    float k = omega * omega;
    float c = 2.0f * omega;
    float a = -k * (*pos - target) - c * (*vel);

    *vel += a * dt;          /* 半隐式欧拉：先速度 */
    *pos += (*vel) * dt;     /* 再用新速度更新位置 */
}
