#ifndef HEX_PHYSICS_H
#define HEX_PHYSICS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * smoothstep。t<=0 返回 0，t>=1 返回 1，区间内单调递增且两端一阶导为 0。
 * 选它而非高斯是为了 MCU 友好：只有乘加，无超越函数。
 */
float hex_smoothstep(float t);

/**
 * 放大镜插值系数。
 * @param d 气泡离焦点的距离（未经径向压缩的世界坐标距离）
 * @param r 影响半径
 * @return 0（正处于中心）~ 1（在影响半径之外）
 */
float hex_magnifier_t(float d, float r);

/**
 * 惯性衰减系数。用时间常数而非每帧固定系数，掉帧时手感不变。
 * @param dt_ms  本帧耗时（毫秒）
 * @param tau_ms 时间常数（毫秒）
 * @return 速度应乘上的系数，位于 (0, 1]
 */
float hex_friction_decay(float dt_ms, float tau_ms);

/**
 * 临界阻尼弹簧的一步积分，就地更新 pos 与 vel。
 * 用半隐式欧拉（先更新速度再更新位置），比显式欧拉稳定得多。
 * @param omega 角频率（rad/s）
 * @param dt    本帧耗时（秒）
 */
void hex_spring_step(float * pos, float * vel, float target, float omega, float dt);

#ifdef __cplusplus
}
#endif
#endif /*HEX_PHYSICS_H*/
