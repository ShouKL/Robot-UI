#pragma once

#include "../../mujoco_thread/mujoco_thread.h"


namespace mujoco {

    enum MotorID {
        LEFT_HIP = 0,      // ���Źؽ�
        RIGHT_HIP = 1,     // ���Źؽ�
        LEFT_THIGH = 2,    // �����
        RIGHT_THIGH = 3,   // �Ҵ���
        LEFT_CALF = 4,     // ��С��
        RIGHT_CALF = 5,    // ��С��
        LEFT_WHEEL = 6,    // ����
        RIGHT_WHEEL = 7    // �ҳ���
    };

    typedef struct {
        std::string name;       // �������
        int id;                 // ���ID
        int dof_id;             // ������õĹؽڵ� Dof ����
        double torque_limit;    // ������λ (N��m)
        double angle_min;       // �Ƕ����� (rad)
        double angle_max;       // �Ƕ����� (rad)
        double target_torque;   // Ŀ������
    } MotorConfig;

    class mj_env : public mujoco_thread
    {

    public:
        MotorConfig motors[8];

        // �ؽ�״̬������ID
        int joint_pos_id[8];
        int joint_vel_id[8];
        mjtNum actual_torques[8];

        // ����/����������ID
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