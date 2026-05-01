#include "mujoco_thread.h"

namespace mujoco{
  mujoco_thread::mujoco_thread(std::string model_file, double max_FPS, int width,
                              int height, std::string title)
      : max_FPS(max_FPS), width(width), height(height), title(title) {
    min_render_time = 1000.0 / max_FPS;
    // load and compile model
    load_model(model_file);
  }

  mujoco_thread::~mujoco_thread() {
    destroyRender();
    // free MuJoCo model and data
    mj_deleteData(d);
    mj_deleteModel(m);
    
    // terminate GLFW (crashes with Linux NVidia drivers)
  #if defined(__APPLE__) || defined(_WIN32)
    glfwTerminate();
  #endif
  }

  void mujoco_thread::load_model(mjModel *m) {
    this->m = new mjModel(*m);
    d = mj_makeData(this->m);
    mj_resetDataKeyframe(this->m, d, 0);
  }

  void mujoco_thread::set_window_size(int width, int height) {
    this->width = width;
    this->height = height;
  }

  void mujoco_thread::set_window_title(std::string title) { this->title = title; }

  void mujoco_thread::set_max_FPS(double max_FPS) {
    this->max_FPS = max_FPS;
    min_render_time = 1000.0 / max_FPS;
  }

  void mujoco_thread::connect_windows_sim() { connect_windows.store(true); }

  void mujoco_thread::load_model(std::string model_file) {
    // load and compile model
    char error[1000] = "Could not load binary model";
    if (model_file.size() > 4 && model_file.compare(model_file.size() - 4, 4, ".mjb") == 0) {
      m = mj_loadModel(model_file.c_str(), 0);
    } else {
      m = mj_loadXML(model_file.c_str(), 0, error, 1000);
    }
    if (!m) {
      mju_error("Load model error: %s", error);
    }

    // make data
    d = mj_makeData(m);
    mj_resetDataKeyframe(m, d, 0);
  }

  void mujoco_thread::sim() {
    // step
    while (is_sim.load()) {

      auto step_start = std::chrono::high_resolution_clock::now();
      if (is_step.load()) {
        std::lock_guard<std::mutex> lk(m_mtx);
        step();
      }
      step_unlock();

      // 同步时间
      auto current_time = std::chrono::high_resolution_clock::now();
      double elapsed_sec =
          std::chrono::duration<double>(current_time - step_start).count();
      double time_until_next_step = m->opt.timestep - elapsed_sec;
      if (time_until_next_step > 0.0) {
        auto sleep_duration = std::chrono::duration<double>(time_until_next_step);
        std::this_thread::sleep_for(sleep_duration);
      }
    }
  }

  void mujoco_thread::step_unlock() {}
  void mujoco_thread::vis_cfg() {}

  void mujoco_thread::render() {
    if (is_show.load())
      return;

    is_show.store(true);
    render_thread = std::thread([this]() {
      initRender(width, height, title.c_str());
      while (is_show.load()) {
        updateRender();
      }
      std::cout << "out render" << std::endl;
      is_render_close.store(true);
      destroyRender();
    });
    render_thread.detach();
  }

  void mujoco_thread::close_render() {
    if (!is_show.load())
      return;
    is_show.store(false);
    while (!is_render_close.load()) {
    }
    destroyRender();
    std::cout << "close render" << std::endl;
  }

  void mujoco_thread::destroyRender() {
    if (is_render_close.load() && !is_show.load()) {
      std::lock_guard<std::mutex> lock(m_mtx);
      if (window != nullptr) {
        // free visualization storage
        mjv_freeScene(&scn);
        mjr_freeContext(&con);
        // 销毁窗口
        glfwDestroyWindow(window);
        window = nullptr;
        is_render_close.store(false);
        // std::cout << "close render" << std::endl;
      }
    }
  }

  void mujoco_thread::initRender(int width, int height, std::string title) {

    if (window != nullptr)
      return;

    // init GLFW
    if (!glfwInit()) {
      mju_error("Could not initialize GLFW");
    }
    // create window, make OpenGL context current, request v-sync
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (window == nullptr) {
      std::cerr << "Failed to create GLFW window" << std::endl;
      glfwTerminate(); // 终止 GLFW
      return;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // initialize visualization data structures
    mjv_defaultCamera(&cam);
    mjv_defaultOption(&opt);
    mjv_defaultScene(&scn);
    mjr_defaultContext(&con);

    // create scene and context
    {
      std::lock_guard<std::mutex> loc(m_mtx);
      mjv_makeScene(m, &scn, 2000);
      mjr_makeContext(m, &con, mjFONTSCALE_150);
    }

    // install GLFW mouse and keyboard callbacks
    // 在初始化窗口后设置回调函数
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, static_keyboard);
    glfwSetCursorPosCallback(window, static_mouse_move);
    glfwSetMouseButtonCallback(window, static_mouse_button);
    glfwSetScrollCallback(window, static_scroll);

    vis_cfg();
  }

  void mujoco_thread::static_keyboard(GLFWwindow *window, int key, int scancode,
                                  int act, int mods) {
    mujoco_thread *instance =
        reinterpret_cast<mujoco_thread *>(glfwGetWindowUserPointer(window));
    if (instance) {
      instance->keyboard(key, scancode, act, mods);
    }
  }

  void mujoco_thread::static_mouse_move(GLFWwindow *window, double xpos,
                                        double ypos) {
    mujoco_thread *instance =
        reinterpret_cast<mujoco_thread *>(glfwGetWindowUserPointer(window));
    if (instance) {
      instance->mouse_move(xpos, ypos);
    }
  }

  void mujoco_thread::static_mouse_button(GLFWwindow *window, int button, int act,
                                          int mods) {
    mujoco_thread *instance =
        reinterpret_cast<mujoco_thread *>(glfwGetWindowUserPointer(window));
    if (instance) {
      instance->mouse_button(button, act, mods);
    }
  }

  void mujoco_thread::static_scroll(GLFWwindow *window, double xoffset,
                                    double yoffset) {
    mujoco_thread *instance =
        reinterpret_cast<mujoco_thread *>(glfwGetWindowUserPointer(window));
    if (instance) {
      instance->scroll(xoffset, yoffset);
    }
  }

  // keyboard callback
  void mujoco_thread::keyboard(int key, int scancode, int act, int mods) {
    // backspace: reset simulation
    if (act == GLFW_PRESS && key == GLFW_KEY_BACKSPACE) {
      mj_resetData(m, d);
      mj_forward(m, d);
    }

    // update Ctrl state
    if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) {
      ctrl_pressed = (act == GLFW_PRESS);
    }
  }

  // mouse button callback
  void mujoco_thread::mouse_button(int button, int act, int mods) {
    // update button state
    button_left =
        (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    button_middle =
        (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
    button_right =
        (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

    // update mouse position
    glfwGetCursorPos(window, &lastx, &lasty);

    // handle double-click selection
    if (act == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) {
      double current_time = glfwGetTime();
      if (current_time - last_click_time < 0.3) {
        // double-click detected
        mjrRect viewport = {0, 0, 0, 0};
        glfwGetFramebufferSize(window, &viewport.width, &viewport.height);
        // unproject screen coordinates to 3D ray
        mjtNum selpnt[3];
        int geomid, flexid, skinid;
        // double click
        selected_body = mjv_select(
            m, d, &opt, (mjtNum)viewport.width / viewport.height,
            lastx / viewport.width, (viewport.height - lasty) / viewport.height,
            &scn, selpnt, &geomid, &flexid, &skinid);
        if (selected_body != -1) {
          // 遍历场景中的几何体
          for (int i = 0; i < scn.ngeom; ++i) {
            // 找到与选中 ID 匹配的几何体
            if (scn.geoms[i].objid == selected_body) {
              std::cout << "Selected body ID: " << selected_body << std::endl;
              // 将几何体的颜色设置为红色
              scn.geoms[i].rgba[0] = 1.0; // 红色
              scn.geoms[i].rgba[1] = 0.0; // 绿色
              scn.geoms[i].rgba[2] = 0.0; // 蓝色
              scn.geoms[i].rgba[3] = 1.0; // 不透明度
              // 如果找到匹配的几何体，可以提前退出循环
              break;
            }
          }
        }
      }
      last_click_time = current_time;
    }
  }

  // mouse move callback
  void mujoco_thread::mouse_move(double xpos, double ypos) {
    // no buttons down: nothing to do
    if (!button_left && !button_middle && !button_right) {
      return;
    }

    // compute mouse displacement, save
    double dx = xpos - lastx;
    double dy = ypos - lasty;
    lastx = xpos;
    lasty = ypos;

    // get current window size
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // get shift key state
    bool mod_shift = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                      glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    // determine action based on mouse button
    mjtMouse action;
    if (button_right) {
      action = mod_shift ? mjMOUSE_MOVE_H : mjMOUSE_MOVE_V;
    } else if (button_left) {
      action = mod_shift ? mjMOUSE_ROTATE_H : mjMOUSE_ROTATE_V;
    } else {
      action = mjMOUSE_ZOOM;
    }

    // move camera
    mjv_moveCamera(m, action, dx / height, dy / height, &scn, &cam);

    // apply force if Ctrl is pressed
    if (ctrl_pressed && selected_body != -1) {
      // Calculate force direction and magnitude
      mjtNum force[3] = {dx * 0.1, dy * 0.1, 0.0};
      // Apply force to the center of the selected body
      mj_applyFT(m, d, force, nullptr, nullptr, selected_body, nullptr);
    }
  }

  // scroll callback
  void mujoco_thread::scroll(double xoffset, double yoffset) {
    // emulate vertical mouse motion = 5% of window height
    mjv_moveCamera(m, mjMOUSE_ZOOM, 0, -0.05 * yoffset, &scn, &cam);
  }

  void mujoco_thread::updateRender() {
    // std::lock_guard<std::mutex> lock(m_mtx);
    if (window != nullptr) {
      if (!glfwWindowShouldClose(window)) {
        // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto step_start = std::chrono::high_resolution_clock::now();

        // get framebuffer viewport
        mjrRect viewport = {0, 0, 0, 0};
        glfwGetFramebufferSize(window, &viewport.width, &viewport.height);
        // update scene and render

        std::unique_lock<std::mutex> lk(m_mtx);
        mjv_updateScene(m, d, &opt, nullptr, &cam, mjCAT_ALL, &scn);

        mjr_render(viewport, &scn, &con);

        std::string fpsText = "FPS: " + std::to_string(fps);
        mjr_overlay(mjFONT_NORMAL, mjGRID_TOPLEFT, viewport, fpsText.c_str(),
                    nullptr, &con);
        
        /*--------绘制--------*/
        draw();
        /*--------绘制--------*/

        lk.unlock();

        // swap OpenGL buffers (blocking call due to v-sync)
        glfwSwapBuffers(window);

        // process pending GUI events, call GLFW callbacks
        glfwPollEvents();

        auto current_time = std::chrono::high_resolution_clock::now();
        auto millis =
            std::chrono::duration<double, std::milli>(current_time - step_start)
                .count();
        if (millis >= min_render_time) {
          // 休眠1ms
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
          int time = (int)(min_render_time - millis);
          std::this_thread::sleep_for(std::chrono::milliseconds(time));
        }

        current_time = std::chrono::high_resolution_clock::now();
        millis =
            std::chrono::duration<double, std::milli>(current_time - step_start)
                .count();
        fps = 1000.0 / millis;
      } else {
        is_show.store(false);
        if (connect_windows.load())
          is_sim.store(false);
      }
    }
  }

  std::vector<mjtNum> mujoco_thread::get_sensor_data(const mjModel *model, const mjData *data,
                                const std::string &sensor_name) {
    int sensor_id = mj_name2id(model, mjOBJ_SENSOR, sensor_name.c_str());
    if (sensor_id == -1) {
      std::cout << "no found sensor" << std::endl;
      return std::vector<mjtNum>();
    }
    int data_pos = model->sensor_adr[sensor_id];
    std::vector<mjtNum> sensor_data(model->sensor_dim[sensor_id]);
    for (int i = 0; i < sensor_data.size(); i++) {
      sensor_data[i] = data->sensordata[data_pos + i];
    }
    return sensor_data;
  }

  void mujoco_thread::get_cam_image(mjvCamera *cam, int width, int height, int stereo) {
    mjrRect viewport2 = {0, 0, width, height};
    int before_stereo = scn.stereo;
    scn.stereo = stereo;
    // mujoco更新渲染
    mjv_updateCamera(m, d, cam, &scn);
    mjr_render(viewport2, &scn, &con);
    scn.stereo = before_stereo;
    // 渲染完成读取图像
    unsigned char *rgbBuffer = new unsigned char[width * height * 3];
    float *depthBuffer = new float[width * height];
    mjr_readPixels(rgbBuffer, depthBuffer, viewport2, &con);
    cv::Mat image(height, width, CV_8UC3, rgbBuffer);
    // 反转图像以匹配OpenGL渲染坐标系
    cv::flip(image, image, 0);
    // 颜色顺序转换这样要使用bgr2rgb而不是rgb2bgr
    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
    cv::imshow("Image", image);
    cv::waitKey(1);
    // 释放内存
    delete[] rgbBuffer;
    delete[] depthBuffer;
  }

  void mujoco_thread::draw_line(mjvScene *scn, mjtNum *from, mjtNum *to, mjtNum width,
                float rgba[4]) {
    scn->ngeom += 1;
    mjvGeom *geom = scn->geoms + scn->ngeom - 1;
    mjv_initGeom(geom, mjGEOM_SPHERE, NULL, NULL, NULL, rgba);
    mjv_connector(geom, mjGEOM_LINE, width, from, to);
  }

  void mujoco_thread::draw_arrow(mjvScene *scn, mjtNum *from, mjtNum *to, mjtNum width,
                  float rgba[4]) {
    scn->ngeom += 1;
    mjvGeom *geom = scn->geoms + scn->ngeom - 1;
    mjv_initGeom(geom, mjGEOM_SPHERE, NULL, NULL, NULL, rgba);
    mjv_connector(geom, mjGEOM_ARROW, width, from, to);
  }

  void mujoco_thread::draw_geom(mjvScene *scn, int type, mjtNum *size, mjtNum *pos, mjtNum *mat,
                float rgba[4]) {
    scn->ngeom += 1;
    mjvGeom *geom = scn->geoms + scn->ngeom - 1;
    mjv_initGeom(geom, type, size, pos, mat, rgba);
  }
}
