#pragma once

#include "mujoco_thread/mujoco_thread.h"


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
        mj_env(std::string model_file, std::string title = "UnderwaterRobot Simulation", double max_FPS = 60);
        ~mj_env();
        void vis_cfg();
        void step();
        void step_unlock();
        void draw();
    };
}