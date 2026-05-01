#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>

// ================== 传感器数据结构 (Sensor) ==================

struct TemperatureSensor {
    double value = 0.0;
};

struct HumiditySensor {
    double value = 0.0;
};

struct DepthSensor {
    double value = 0.0;
};

struct SensorData {
    TemperatureSensor temperature;
    HumiditySensor humidity;
    DepthSensor depth;
    bool is_valid = false; 
};

// ================== 执行器数据结构 (Actuator) ==================

struct ThrustCurve {
    double np_mid = 0, np_ini = 0, pp_ini = 0, pp_mid = 0;
    double nt_end = 0, nt_mid = 0, pt_mid = 0, pt_end = 0;
};

struct BrushlessMotor {
    int id = 0;
    ThrustCurve curve;
    double target_speed = 0.0;
};

struct Servo {
    int id = 0;
    double angle = 0.0;
};

struct MotionControl {
    double x = 0, y = 0, z = 0;
    double rx = 0, ry = 0, rz = 0;
};

struct ActuatorData {
    std::map<int, BrushlessMotor> brushlessmotor;
    std::map<int, Servo> servo;
    MotionControl motion;
};


class RobotAPI {
public:
    virtual ~RobotAPI() = default;

    virtual bool Initialize(const std::string& host_ip, int remote_port, int local_port) = 0;
    
    virtual bool HardwareInit(int max_retries = 3) = 0;

    // 固定数据传输格式为 Actuator / Sensor
    virtual SensorData GetSensorData() = 0;
    virtual void SendActuatorData(const ActuatorData& data) = 0;
};