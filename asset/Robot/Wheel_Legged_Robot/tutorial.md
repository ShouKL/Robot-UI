# Ray
射线测距接口
![](../../MJCF/asset/ray.png)
void mj_multiRay(const mjModel* m, mjData* d, const mjtNum pnt[3], const mjtNum* vec,
                 const mjtByte* geomgroup, mjtByte flg_static, int bodyexclude,
                 int* geomid, mjtNum* dist, int nray, mjtNum cutoff);
* m: mjModel
* d: mjData
* pnt: 射线起点
* vec: 射线向量
* geomgroup：检测分组，NULL代表检测所有
* flg_static: 是否检测静态物体 1检测 0不检测
* bodyexclude: -1 可用于指示包括所有主体
* geomid: 检测到的几何体id，没有为-1
* dist: 到geom表面的距离，数据是距离比例，是vec的倍数，无限远为-1
* nray：射线数量
* cutoff: 距离截断，超过截断距离的射线不检测，判断条件(pnt到geompos中心>cutoff+几何体外接球半径)
**其余ratXXX参数同理**