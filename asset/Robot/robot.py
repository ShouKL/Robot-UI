import mujoco
import mujoco.viewer
import numpy as np
import time

# === 配置参数 ===
XML_PATH = "scene.xml"  # 你的XML文件名
WATER_LEVEL = 2.0     # 水面高度 (Z=0)
WATER_DENSITY = 1000.0 # 水的密度 kg/m^3
GRAVITY = 9.81

# 浮力部件的名称（对应 XML 中的 geom name）
# 你可以把所有能产生浮力的 geom 名字都放进去
BUOYANCY_GEOMS = ["robot____fu_li_cai_qian__1","robot____fu_li_cai_qian__2",
                  "robot____fu_li_cai_hou__1","robot____fu_li_cai_hou__2",
                  "robot____cang_ti__1__cang_qian_gai__1","robot____cang_ti__1__cang_ke__1"
                  ] 

def apply_buoyancy(model, data):
    """
    计算并应用阿基米德浮力
    """
    for i in range(model.ngeom):
        name = mujoco.mj_id2name(model, mujoco.mjtObj.mjOBJ_GEOM, i)
        
        if name in BUOYANCY_GEOMS:
            # 1. 获取该 Geom 的位置和姿态
            geom_pos = data.geom_xpos[i]
            
            # 2. 获取 Geom 的形状参数计算体积 (这里简化为 Box)
            # model.geom_size[i] 存储的是 [长/2, 宽/2, 高/2]
            # Box 体积 = (2*x) * (2*y) * (2*z)
            # 注意：如果用 Mesh，需要预先算好体积填在这里
            g_size = model.geom_size[i]
            volume = (g_size[0]*2) * (g_size[1]*2) * (g_size[2]*2)
            
            # 3. 判断是否在水下
            # 简单模型：比较中心点高度。
            # 进阶模型：应该计算浸没比例。这里为了演示，只要中心在水下就给满浮力。
            if geom_pos[2] < WATER_LEVEL:
                # 计算浮力大小 F = rho * g * V
                buoyancy_force = 10*WATER_DENSITY * GRAVITY * volume
                
                # 4. 增加水阻力 (Drag) - 防止它像弹簧一样永远弹跳
                # 获取该 geom 所属 body 的速度
                body_id = model.geom_bodyid[i]
                vel = data.cvel[body_id] # 6维速度向量
                linear_vel = vel[3:6]
                
                # 阻力方向与速度相反，大小与速度平方成正比
                drag_factor = 50.0 # 阻力系数，根据手感调
                drag_force = -drag_factor * linear_vel 
                
                # 5. 施加力
                # xfrc_applied 是作用在 body 重心的外力 [fx, fy, fz, tx, ty, tz]
                total_force = [0, 0, buoyancy_force, 0, 0, 0] + \
                              [drag_force[0], drag_force[1], drag_force[2], 0, 0, 0]
                
                data.xfrc_applied[body_id] = total_force
            else:
                # 在空气中，清除外力（只受重力）
                body_id = model.geom_bodyid[i]
                data.xfrc_applied[body_id] = np.zeros(6)

# === 主循环 ===
model = mujoco.MjModel.from_xml_path(XML_PATH)
data = mujoco.MjData(model)

with mujoco.viewer.launch_passive(model, data) as viewer:
    start = time.time()
    while viewer.is_running():
        step_start = time.time()

        # --- 核心：在物理步进前计算浮力 ---
        apply_buoyancy(model, data)
        # -------------------------------

        mujoco.mj_step(model, data)
        viewer.sync()

        # 保持实时模拟速度
        time_until_next_step = model.opt.timestep - (time.time() - step_start)
        if time_until_next_step > 0:
            time.sleep(time_until_next_step)