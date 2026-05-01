#include "utils/mujoco_thread/mujoco_thread.h"

namespace mujoco {

  enum MotorID {
    LEFT_HIP = 0,      // 左髋关节
    RIGHT_HIP = 1,     // 右髋关节
    LEFT_THIGH = 2,    // 左大腿
    RIGHT_THIGH = 3,   // 右大腿
    LEFT_CALF = 4,     // 左小腿
    RIGHT_CALF = 5,    // 右小腿
    LEFT_WHEEL = 6,    // 左车轮
    RIGHT_WHEEL = 7    // 右车轮
  };

  typedef struct {
    std::string name;       // 电机名称
    int id;                 // 电机ID
    int dof_id;             // 电机作用的关节的 Dof 索引
    double torque_limit;    // 力矩限位 (N·m)
    double angle_min;       // 角度下限 (rad)
    double angle_max;       // 角度上限 (rad)
    double target_torque;   // 目标力矩
  } MotorConfig;

  class mj_env : public mujoco_thread 
  {

  public:
    MotorConfig motors[8]; 

    // 关节状态传感器ID
    int joint_pos_id[8];
    int joint_vel_id[8];
    mjtNum actual_torques[8];

    // 基座/环境传感器ID
    int base_ori_id;          // base_orientation
    int base_angvel_id;       // base_angular_velocity
    int base_linacc_id;       // base_linear_acceleration
    int base_linvel_id;       // base_linear_velocity
    int base_pos_id;          // base_position

    // mjvCamera cam2;
    // mjtNum *cam_pos;

    // mjtNum num_vec[box_num][3];
    // mjtNum dist_ratio[box_num]={0.0};

    // int touch_point_adr[20][20];
    // cv::Mat touch;

  public:
    mj_env(std::string model_file, double max_FPS = 60) {
      load_model(model_file);
      set_window_size(1920, 1080);
      set_window_title("Wheel Legged Robot");
      set_max_FPS(max_FPS);



      // 1. 髋关节电机（左右摆，π/4限位）
      motors[LEFT_HIP]   = {"left_hip_motor", -1, -1, 60.0, -M_PI/4, M_PI/4, 0.0};
      motors[RIGHT_HIP]  = {"right_hip_motor", -1, -1, 60.0, -M_PI/4, M_PI/4, 0.0};
      // 2. 大腿电机（俯仰，π/4限位）
      motors[LEFT_THIGH] = {"left_thigh_motor", -1, -1, 50.0, 0.0, M_PI/2, 0.0};
      motors[RIGHT_THIGH]= {"right_thigh_motor", -1, -1, 50.0, 0.0, M_PI/2, 0.0};
      // 3. 小腿电机（微调姿态，限位-π/6~π/6）
      motors[LEFT_CALF]  = {"left_calf_motor", -1, -1, 40.0, -M_PI/4, M_PI/2, 0.0};
      motors[RIGHT_CALF] = {"right_calf_motor", -1, -1, 40.0, -M_PI/4, M_PI/2, 0.0};
      // 4. 车轮电机（无角度限位）
      motors[LEFT_WHEEL] = {"left_wheel_motor", -1, -1, 12.0, -1e9, 1e9, 0.0};
      motors[RIGHT_WHEEL]= {"right_wheel_motor", -1, -1, 12.0, -1e9, 1e9, 0.0};

      // 获取每个电机的ID（关联XML中的actuator）
      for (int i = 0; i < 8; i++){
        motors[i].id = mj_name2id(m, mjOBJ_ACTUATOR, motors[i].name.c_str());
        int joint_id = m->actuator_trnid[motors[i].id * 2];
        motors[i].dof_id = m->jnt_dofadr[joint_id];
      }

     
      // //相机初始化
      // int camID = mj_name2id(m, mjOBJ_CAMERA, "look_box");
      // if (camID == -1) {
      //   std::cerr << "Camera not found" << std::endl;
      // } else {
      //   mjv_defaultCamera(&cam2);
      //   cam2.fixedcamid = camID;
      //   cam2.type = mjCAMERA_FIXED;
      // }
      // // 获取摄像机的位置
      // cam_pos = &m->cam_pos[3 * camID];
      // std::cout << "Camera Position: (" << cam_pos[0] << ", " << cam_pos[1] << ", "
      //           << cam_pos[2] << ")" << std::endl;

      {
        /*--------box--------*/
        // //获取施加力的body的id
        // int box_id = mj_name2id(m, mjOBJ_BODY, "box");
        // //施加外部力
        // mjtNum box_force[3] = {0.0, 0.0, 9.8};
        // mjtNum box_torque[3] = {0.0, 0.0, 0.0};
        // mjtNum box_point[3] = {1.0, 0.0, 0.2};
        // int red_point = mj_name2id(m, mjOBJ_SITE, "red_point");
        // mjtNum *point = d->site_xpos + red_point * 3;
        // // mj_applyFT(m, d, box_force, box_torque, box_point, box_id, d->qfrc_applied);
        /*--------box--------*/

        /*--------力——加速度--------*/
        // //获取施加力的body的id
        // int sphere_id = mj_name2id(m, mjOBJ_BODY, "sphere");
        // //施加外部力
        // mjtNum sphere_force[3] = {0.3, 0.0, 0.0};
        // mjtNum sphere_torque[3] = {0.0, 0.0, 0.0};
        // mjtNum sphere_point[3] = {0.0, 0.0, 0.0};
        // // mj_applyFT(m, d, sphere_force, sphere_torque, sphere_point, sphere_id,
        // //            d->qfrc_applied);
        /*--------力——加速度--------*/

        /*--------扭矩--------*/
        // int pointer_id = mj_name2id(m, mjOBJ_BODY, "pointer");
        // //施加外部力
        // mjtNum pointer_force[3] = {0.0, 0.0, 0.0};
        // mjtNum pointer_torque[3] = {0.0, 0.0, 0.0};
        // mjtNum pointer_point[3] = {0.0, 0.0, 0.0};
        // mj_applyFT(m, d, pointer_force, pointer_torque, pointer_point, pointer_id,
        //            d->qfrc_applied);
        /*--------扭矩--------*/
      }

      /*--------读取传感器--------*/
      std::string joint_names[8] = {
        "left_hip", "right_hip", "left_thigh", "right_thigh", 
        "left_calf", "right_calf", "left_wheel", "right_wheel"
      };

      for (int i = 0; i < 8; ++i) {
          std::string prefix = joint_names[i];
          
          // 位置 ID
          joint_pos_id[i] = mj_name2id(m, mjOBJ_SENSOR, (prefix + "_joint_p").c_str());
          // 速度 ID
          joint_vel_id[i] = mj_name2id(m, mjOBJ_SENSOR, (prefix + "_joint_v").c_str());
      }
      
      // 基座/环境传感器 ID
      base_ori_id          = mj_name2id(m, mjOBJ_SENSOR, "base_orientation");
      base_angvel_id       = mj_name2id(m, mjOBJ_SENSOR, "base_angular_velocity");
      base_linacc_id       = mj_name2id(m, mjOBJ_SENSOR, "base_linear_acceleration");
      base_linvel_id       = mj_name2id(m, mjOBJ_SENSOR, "base_linear_velocity");
      base_pos_id          = mj_name2id(m, mjOBJ_SENSOR, "base_position");
      /*--------读取传感器--------*/
    };

    
    void vis_cfg() {
      /*--------可视化配置--------*/
      // opt.flags[mjtVisFlag::mjVIS_CONTACTPOINT] = true;
      // opt.flags[mjtVisFlag::mjVIS_CONTACTFORCE] = true;
      // opt.flags[mjtVisFlag::mjVIS_CAMERA] = true;
      // opt.flags[mjtVisFlag::mjVIS_CONVEXHULL] = true;
      // opt.flags[mjtVisFlag::mjVIS_COM] = true;
      // opt.label = mjtLabel::mjLABEL_BODY;
      // opt.frame = mjtFrame::mjFRAME_WORLD;
      /*--------可视化配置--------*/

      /*--------场景渲染--------*/
      // scn.flags[mjtRndFlag::mjRND_WIREFRAME] = true;
      // scn.flags[mjtRndFlag::mjRND_SEGMENT] = true;
      scn.flags[mjtRndFlag::mjRND_IDCOLOR] = true;
      /*--------场景渲染--------*/
    }

    void step() {
      /*--------单射线--------*/
      // mjtNum *box_pos = d->geom_xpos + box_id * 3;
      // mjtNum *box2_pos = d->geom_xpos + box2_id * 3;
      // mjtNum vec1[3], vec2[3];
      // for (int i = 0; i < 3; i++) {
      //   vec1[i] = box_pos[i] - cam_pos[i];
      //   vec2[i] = box2_pos[i] - cam_pos[i];
      // }
      // int geomid[1];
      // mjtNum x1 = mj_ray(m, d, cam_pos, vec1, NULL, 1, -1, geomid);
      // mjtNum x2 = mj_ray(m, d, cam_pos, vec2, NULL, 1, -1, geomid);
      // mjtNum distance1 = mju_norm3(vec1) * x1;
      // mjtNum distance2 = mju_norm3(vec2) * x2;
      // // std::cout << "x1: " << x1 << "  x2: " << x2 << std::endl;
      // // std::cout << "distance1: " << distance1 << "  distance2: " << distance2
      // //           << std::endl;
      /*--------单射线--------*/

      /*--------多射线--------*/
      // for (int i = 0; i < box_num; i++) {
      //   for (int j = 0; j < 3; j++) {
      //     num_vec[i][j] = boxs_pos[i][j] - cam_pos[j];
      //   }
      // }
      // int geomids[box_num]={0};
    
      // mjtNum dist[box_num]={0.0};
      // mj_multiRay(m, d, cam_pos, (const mjtNum *)num_vec, NULL, 1, -1, geomids,
      // dist_ratio, box_num, 999);
      // for (int i = 0; i < box_num; i++) {
      //   if(geomids[i]==-1)
      //   {
      //     dist[i] = -1;
      //     continue;
      //   }
      //   dist[i] = mju_norm3(num_vec[i]) * dist_ratio[i];
      // }
      // std::cout << "multiRay distance: ";
      // for (int i = 0; i < box_num; i++) {
      //   std::cout << dist[i] << " ";
      // }
      // std::cout << std::endl;
      /*--------多射线--------*/


      // get_cam_image(&cam2,1024,640,mjtStereo::mjSTEREO_SIDEBYSIDE);


      {
        /*--------box--------*/
        // mju_zero(d->qfrc_applied, m->nv);
        // mj_applyFT(m, d, box_force, box_torque, point, box_id, d->qfrc_applied);
        // mj_step(m, d);
        /*--------box--------*/

        /*--------box--------*/
        // mjtNum *box_xfrc_applied = d->xfrc_applied + box_id * 6;
        // box_xfrc_applied[0] = 0.0; // fx
        // box_xfrc_applied[1] = 0.0; // fy
        // box_xfrc_applied[2] = 9.8; // fz
        // box_xfrc_applied[3] = 0.0; // tx
        // box_xfrc_applied[4] = 0.0; // ty
        // box_xfrc_applied[5] = 0.0; // tz
        // mj_step(m, d);
        /*--------box--------*/

        /*--------力——加速度--------*/
        // d->ctrl[0] = 0.6;
        // mjtNum *sphere_xfrc_applied = d->xfrc_applied + sphere_id * 6;
        // sphere_xfrc_applied[0] = 0.0; // fx
        // sphere_xfrc_applied[1] = 0.0; // fy
        // sphere_xfrc_applied[2] = 0.0; // fz
        // sphere_xfrc_applied[3] = 0.0; // tx
        // sphere_xfrc_applied[4] = 0.0; // ty
        // sphere_xfrc_applied[5] = 0.0; // tz
        // mj_step(m, d);
        // std::cout << "qfrc_passive:" << d->qfrc_passive[0]
        //           << "  qfrc_actuator:" << d->qfrc_actuator[0]
        //           << "  qfrc_applied:" << d->qfrc_applied[0]
        //           << "  qfrc_bias:" << d->qfrc_bias[0]
        //           << "  efc_force:" << d->efc_force[0] << std::endl;
        // auto lin_acc = get_sensor_data(m, d, "lin_acc");
        // std::cout << "lin_acc:" << lin_acc[0] << std::endl;
        // auto lin_vel = get_sensor_data(m, d, "lin_vel");
        // std::cout << "lin_vel:" << lin_vel[0] << std::endl;
        // auto lin_pos = get_sensor_data(m, d, "lin_pos");
        // std::cout << "lin_pos:" << lin_pos[0] << std::endl;
        // mjtNum acc = (d->qfrc_passive[0] + d->qfrc_actuator[0] +
        //               d->qfrc_applied[0] + d->qfrc_bias[0] + d->efc_force[0]) /
        //              m->body_mass[sphere_id];
        // std::cout << "计算加速度:" << acc << std::endl;
        /*--------力——加速度--------*/

        /*--------扭矩--------*/
        // d->ctrl[1] = 0.6;
        // mjtNum *pointer_xfrc_applied = d->xfrc_applied + pointer_id * 6;
        // pointer_xfrc_applied[0] = 0.0; // fx
        // pointer_xfrc_applied[1] = 0.0; // fy
        // pointer_xfrc_applied[2] = 0.0; // fz
        // pointer_xfrc_applied[3] = 0.0; // tx
        // pointer_xfrc_applied[4] = 0.0; // ty
        // pointer_xfrc_applied[5] = 0.0; // tz
        // mj_step(m, d);
        // std::cout << "qfrc_passive:" << d->qfrc_passive[1]
        //           << "  qfrc_actuator:" << d->qfrc_actuator[1]
        //           << "  qfrc_applied:" << d->qfrc_applied[1]
        //           << "  qfrc_bias:" << d->qfrc_bias[1]
        //           << "  efc_force:" << d->efc_force[1] << std::endl;
        // mjtNum tau = d->qfrc_passive[1] + d->qfrc_actuator[1] + d->qfrc_applied[1] +
        //             d->qfrc_bias[1] + d->efc_force[1];
        // std::cout << "计算扭矩:" << tau << std::endl;
        // auto t = get_sensor_data(m, d, "torque");
        // std::cout << "测量扭矩:" << t[2] << std::endl;
        // auto pivot_pos = get_sensor_data(m, d, "pivot_pos");
        // std::cout << "pivot_pos:" << pivot_pos[0] << std::endl;
        // auto pivot_vel = get_sensor_data(m, d, "pivot_vel");
        // std::cout << "pivot_vel:" << pivot_vel[0] << std::endl;
        /*--------扭矩--------*/

        d->ctrl[motors[LEFT_HIP].id] = 5;       // 左髋控制量
        d->ctrl[motors[RIGHT_HIP].id] = 5;      // 右髋控制量
        d->ctrl[motors[LEFT_THIGH].id] = -20;   // 左大腿控制量
        d->ctrl[motors[RIGHT_THIGH].id] = -20;  // 右大腿控制量
        d->ctrl[motors[LEFT_CALF].id] = 5;      // 左小腿控制量
        d->ctrl[motors[RIGHT_CALF].id] = 5;     // 右小腿控制量
        d->ctrl[motors[LEFT_WHEEL].id] = 1;     // 左车轮控制量
        d->ctrl[motors[RIGHT_WHEEL].id] = 1;    // 右车轮控制量
        mj_step(m, d);
        for (int i = 0; i < 8; i++){
          actual_torques[i] = d->qfrc_actuator[motors[i].dof_id];
        }
      }

      // touch = cv::Mat(20, 20, CV_8UC1);
      // for (int x = 0; x < 20; x++) {
      //   for (int y = 0; y < 20; y++) {
      //     int idx = x * 20 + y;
      //     mjtNum data = mju_norm3(d->sensordata + touch_point_adr[x][y]);
      //     data = mju_clip(data, 0.0, 3.0);
      //     touch.at<uchar>(x, y) = data / 3 * 255;
      //   }
      // }

    }

    void step_unlock() {

      // cv::resize(touch, touch, cv::Size(200, 200));
      // cv::imshow("touch", touch);
      // cv::waitKey(1);

    }

    void draw() {
      /*--------3D绘制--------*/
      // mjtNum from[3] = {0, 1, 1};
      // mjtNum to[3] = {0, 3, 4};
      // float color[4] = {0, 1, 0, 1};
      // draw_line(&scn, from, to, 20, color);
      // mjtNum to2[3] = {0, -1, 1};
      // float color2[4] = {0, 0, 1, 1};
      // draw_arrow(&scn, from, to2, 0.1, color2);

      // mjtNum size[3] = {0.1, 0, 0};
      // mjtNum pos[3] = {0, 0, 1.0};
      // mjtNum mat[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};//坐标系，空间向量
      // draw_geom(&scn, mjGEOM_SPHERE, size, pos, mat, color);
      /*--------3D绘制--------*/


      /*--------速度跟踪--------*/
      // auto lin_vel = get_sensor_data(m, d, "base_lin_vel");
      // auto base_pos = get_sensor_data(m, d, "base_pos");
      // for (int i = 0; i < 3; i++) {
      //   from[i] = base_pos[i];
      //   to[i] = base_pos[i] + lin_vel[i] * 5;
      // }
      // from[2] += 0.5;
      // to[2] += 0.5;
      // float color3[4] = {0.3, 0.6, 0.3, 0.9};
      // draw_arrow(&scn, from, to, 0.1, color3);
      /*--------速度跟踪--------*/


      /*--------设置分割颜色--------*/
      // mjvGeom *geom;
      // std::cout << scn.ngeom << std::endl;
      // for (int i = 0; i < scn.ngeom; i++) {
      //   geom = scn.geoms + i;
      //   if (geom->objid == box_id && geom->objtype == mjOBJ_GEOM)
      //     break;
      // }
      // uint32_t r = 254;
      // uint32_t g = 0;
      // uint32_t b = 255;
      // geom->segid = (b << 16) | (g << 8) | r;
      // std::cout << geom->segid << std::endl;
      /*--------设置分割颜色--------*/


      // float rgba[4] = {0, 0, 1, 0.5};
      // for (int i = 0; i < box_num; i++) {
      //   mjtNum extremity_pos[3];
      //   extremity_pos[0] = cam_pos[0] + num_vec[i][0] * dist_ratio[i];
      //   extremity_pos[1] = cam_pos[1] + num_vec[i][1] * dist_ratio[i];
      //   extremity_pos[2] = cam_pos[2] + num_vec[i][2] * dist_ratio[i];
      //   draw_line(&scn, cam_pos, extremity_pos, 2, rgba);
      // }

      /*--------2D绘制--------*/
      // mjr_text(mjFONT_NORMAL, "Albusgive", &con, 0, 0.9, 1, 0, 1);
      // mjrRect viewport2 = {50, 100, 50, 50};
      // mjr_overlay(mjFONT_NORMAL, mjGRID_TOPLEFT, viewport2, "github", "Albusgive",
      //             &con);
      // mjr_rectangle(viewport2, 0.5, 0, 1, 0.6);
      // mjrRect viewport3 = {100, 200, 150, 50};
      // mjr_label(viewport3, mjFONT_NORMAL, "Albusgive", 0, 1, 1, 1, 0, 0, 0, &con);
      /*--------2D绘制--------*/

      /*-------- 传感器数据显示--------*/
#define GET_SENSOR_PTR(id) ((id) >= 0 && (id) < m->nsensor) ? (d->sensordata + m->sensor_adr[(id)]) : nullptr

      mjrRect viewport = {0, -60, 0, 0}; 
      glfwGetFramebufferSize(window, &viewport.width, &viewport.height);
      char buffer[256];
      
      // 关节名称数组
      std::string joint_labels[8] = {
          "L Hip", "R Hip", "L Thigh", "R Thigh", 
          "L Calf", "R Calf", "L Wheel", "R Wheel"
      };

      // 关节状态
      mjr_overlay(mjFONT_NORMAL, mjGRID_TOPLEFT, viewport, "JOINT STATES:", " ", &con);

      for (int i = 0; i < 8; ++i) {
          const mjtNum* pos = GET_SENSOR_PTR(joint_pos_id[i]);
          const mjtNum* vel = GET_SENSOR_PTR(joint_vel_id[i]);
          if (pos) {
              snprintf(buffer, sizeof(buffer), "%s Pos: \t%.3f rad", joint_labels[i].c_str(), pos[0]);
              viewport.bottom -= 35;
              mjr_overlay(mjFONT_NORMAL, mjGRID_TOPLEFT, viewport, "", buffer, &con);
          }
          if (vel) {
              snprintf(buffer, sizeof(buffer), "%s Vel: \t%.3f r/s", joint_labels[i].c_str(), vel[0]);
              viewport.bottom -= 35;
              mjr_overlay(mjFONT_NORMAL, mjGRID_TOPLEFT, viewport, "", buffer, &con);
          }
          snprintf(buffer, sizeof(buffer), "%s Trq: %.2f Nm", joint_labels[i].c_str(), actual_torques[i]);
          viewport.bottom -= 35;
          mjr_overlay(mjFONT_NORMAL, mjGRID_TOPLEFT, viewport, "", buffer, &con);
      }
      // 基座/环境传感器
      viewport.bottom = -60;
      mjr_overlay(mjFONT_NORMAL, mjGRID_TOPRIGHT, viewport, "BASE/IMU STATES:", " ", &con);

      // 基座姿态 (Base Orientation) - 四元数
      const mjtNum* ori_data = GET_SENSOR_PTR(base_ori_id);
      if (ori_data) {
          snprintf(buffer, sizeof(buffer), "ORI (w,x,y,z): \t%.2f | \t%.2f \t%.2f \t%.2f", 
                  ori_data[0], ori_data[1], ori_data[2], ori_data[3]);
          viewport.bottom -= 35;
          mjr_overlay(mjFONT_NORMAL, mjGRID_TOPRIGHT, viewport, "", buffer, &con);
      }

      // 基座角速度 (Base Angular Velocity) - 陀螺仪
      const mjtNum* angvel_data = GET_SENSOR_PTR(base_angvel_id);
      if (angvel_data) {
          snprintf(buffer, sizeof(buffer), "AngVel (x,y,z): \t%.2f | \t%.2f | \t%.2f", 
                  angvel_data[0], angvel_data[1], angvel_data[2]);
          viewport.bottom -= 35;
          mjr_overlay(mjFONT_NORMAL, mjGRID_TOPRIGHT, viewport, "", buffer, &con);
      }
      
      // 基座线加速度 (Base Linear Acceleration) - 加速度计
      const mjtNum* linacc_data = GET_SENSOR_PTR(base_linacc_id);
      if (linacc_data) {
          snprintf(buffer, sizeof(buffer), "LinAcc (x,y,z): \t%.2f | \t%.2f | \t%.2f", 
                  linacc_data[0], linacc_data[1], linacc_data[2]);
          viewport.bottom -= 35;
          mjr_overlay(mjFONT_NORMAL, mjGRID_TOPRIGHT, viewport, "", buffer, &con);
      }

      // 基座线速度 (Base Linear Velocity)
      const mjtNum* linvel_data = GET_SENSOR_PTR(base_linvel_id);
      if (linvel_data) {
          snprintf(buffer, sizeof(buffer), "LinVel (x,y,z): \t%.2f | \t%.2f | \t%.2f", 
                  linvel_data[0], linvel_data[1], linvel_data[2]);
          viewport.bottom -= 35;
          mjr_overlay(mjFONT_NORMAL, mjGRID_TOPRIGHT, viewport, "", buffer, &con);
      }

      // 基座位置 (Base Position)
      const mjtNum* pos_data_base = GET_SENSOR_PTR(base_pos_id);
      if (pos_data_base) {
          snprintf(buffer, sizeof(buffer), "Pos (x,y,z): \t%.2f | \t%.2f | \t%.2f", 
                  pos_data_base[0], pos_data_base[1], pos_data_base[2]);
          viewport.bottom -= 35;
          mjr_overlay(mjFONT_NORMAL, mjGRID_TOPRIGHT, viewport, "", buffer, &con);
      }
      /*-------- 传感器数据显示--------*/
    }
  };
}

// main function
int main(int argc, const char **argv) {
  mujoco::mj_env mujoco("/home/shoukeling/mujoco_Project/API-MJCF/Wheel_Legged_Robot/scene.xml", 170);
  mujoco.connect_windows_sim();
  mujoco.render();
  mujoco.sim();

  return 0;
}
