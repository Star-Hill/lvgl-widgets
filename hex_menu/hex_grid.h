#ifndef HEX_GRID_H
#define HEX_GRID_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 平铺周期。37 = 4² + 4·3 + 3² 是 Löschian 数，因此 37 个格子能在无限
 * 六边形网格上构成完美的环面铺砌——朝任意方向拖拽图案都严丝合缝地重复。
 * 半径为 3 的正六边形恰好就是它的基本域。
 */
#define HEX_SLOT_CNT 37

/** pointy-top 轴向坐标 */
typedef struct {
    int q;
    int r;
} hex_axial_t;

/**
 * 轴向坐标 -> 格心像素（世界坐标）
 * @param s 网格特征尺寸（六边形外接圆半径）。列间距 s·√3，行间距 s·1.5
 */
void hex_axial_to_px(hex_axial_t a, float s, float * out_x, float * out_y);

/** 像素（世界坐标）-> 最近的轴向格子 */
hex_axial_t hex_px_to_axial(float x, float y, float s);

/**
 * 轴向坐标 -> 0..HEX_SLOT_CNT-1 的槽位索引。
 * 满足晶格平移不变性：坐标加上 (4,3) 或 (-3,7) 的任意整数倍，返回值不变。
 */
int hex_slot_index(int q, int r);

#ifdef __cplusplus
}
#endif
#endif /*HEX_GRID_H*/
