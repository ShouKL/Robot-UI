// Custom Protocol Callback
// Write your protocol formatting logic inside the functions below.

#include "Robot_API/robot_api.h"

void EncodeDataFrame(const ActuatorData& data, char* out_buffer, int max_len) {
    // Write your data encoding logic here.
    // Use variables from `data` such as data.brushlessmotor and data.servo.
    // Example:
    // snprintf(out_buffer, max_len, "Motor0: %.2f\r\n", data.brushlessmotor.at(0).target_speed);
}

void DecodeDataFrame(const char* in_buffer, int len, SensorData& out_data) {
    // Write your data decoding logic and checksum validation here.
}
