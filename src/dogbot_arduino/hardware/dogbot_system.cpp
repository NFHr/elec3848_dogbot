// Copyright 2021 ros2_control Development Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

#include "dogdrive_arduino/dogbot_system.hpp"

namespace dogdrive_arduino
{
  hardware_interface::CallbackReturn DogdriveArduinoHardware::on_init(
      const hardware_interface::HardwareInfo &info)
  {
    if (
        hardware_interface::SystemInterface::on_init(info) !=
        hardware_interface::CallbackReturn::SUCCESS)
    {
      return hardware_interface::CallbackReturn::ERROR;
    }

    cfg_.lf_wheel_name = info_.hardware_parameters["lf_wheel_name"];
    cfg_.rf_wheel_name = info_.hardware_parameters["rf_wheel_name"];
    cfg_.lb_wheel_name = info_.hardware_parameters["lb_wheel_name"];
    cfg_.rb_wheel_name = info_.hardware_parameters["rb_wheel_name"];
    cfg_.loop_rate = std::stof(info_.hardware_parameters["loop_rate"]);
    cfg_.device = info_.hardware_parameters["device"];
    cfg_.baud_rate = std::stoi(info_.hardware_parameters["baud_rate"]);
    cfg_.timeout_ms = std::stoi(info_.hardware_parameters["timeout_ms"]);
    cfg_.enc_counts_per_rev = std::stoi(info_.hardware_parameters["enc_counts_per_rev"]);
    if (info_.hardware_parameters.count("pid_p") > 0)
    {
      cfg_.pid_p = std::stoi(info_.hardware_parameters["pid_p"]);
      cfg_.pid_d = std::stoi(info_.hardware_parameters["pid_d"]);
      cfg_.pid_i = std::stoi(info_.hardware_parameters["pid_i"]);
      cfg_.pid_o = std::stoi(info_.hardware_parameters["pid_o"]);
    }
    else
    {
      RCLCPP_INFO(rclcpp::get_logger("DogdriveArduinoHardware"), "PID values not supplied, using defaults.");
    }

    wheel_lf_.setup(cfg_.lf_wheel_name, cfg_.enc_counts_per_rev);
    wheel_rf_.setup(cfg_.rf_wheel_name, cfg_.enc_counts_per_rev);
    wheel_lb_.setup(cfg_.lb_wheel_name, cfg_.enc_counts_per_rev);
    wheel_rb_.setup(cfg_.rb_wheel_name, cfg_.enc_counts_per_rev);

    for (const hardware_interface::ComponentInfo &joint : info_.joints)
    {
      // DogbotSystem has exactly two states and one command interface on each joint (wheel)
      if (joint.command_interfaces.size() != 1)
      {
        RCLCPP_FATAL(
            rclcpp::get_logger("DogdriveArduinoHardware"),
            "Joint '%s' has %zu command interfaces found. 1 expected.", joint.name.c_str(),
            joint.command_interfaces.size());
        return hardware_interface::CallbackReturn::ERROR;
      }

      if (joint.command_interfaces[0].name != hardware_interface::HW_IF_VELOCITY)
      {
        RCLCPP_FATAL(
            rclcpp::get_logger("DogdriveArduinoHardware"),
            "Joint '%s' have %s command interfaces found. '%s' expected.", joint.name.c_str(),
            joint.command_interfaces[0].name.c_str(), hardware_interface::HW_IF_VELOCITY);
        return hardware_interface::CallbackReturn::ERROR;
      }

      if (joint.state_interfaces.size() != 2)
      {
        RCLCPP_FATAL(
            rclcpp::get_logger("DogdriveArduinoHardware"),
            "Joint '%s' has %zu state interface. 2 expected.", joint.name.c_str(),
            joint.state_interfaces.size());
        return hardware_interface::CallbackReturn::ERROR;
      }

      if (joint.state_interfaces[0].name != hardware_interface::HW_IF_POSITION)
      {
        RCLCPP_FATAL(
            rclcpp::get_logger("DogdriveArduinoHardware"),
            "Joint '%s' have '%s' as first state interface. '%s' expected.", joint.name.c_str(),
            joint.state_interfaces[0].name.c_str(), hardware_interface::HW_IF_POSITION);
        return hardware_interface::CallbackReturn::ERROR;
      }

      if (joint.state_interfaces[1].name != hardware_interface::HW_IF_VELOCITY)
      {
        RCLCPP_FATAL(
            rclcpp::get_logger("DogdriveArduinoHardware"),
            "Joint '%s' have '%s' as second state interface. '%s' expected.", joint.name.c_str(),
            joint.state_interfaces[1].name.c_str(), hardware_interface::HW_IF_VELOCITY);
        return hardware_interface::CallbackReturn::ERROR;
      }
    }

    return hardware_interface::CallbackReturn::SUCCESS;
  }

  std::vector<hardware_interface::StateInterface> DogdriveArduinoHardware::export_state_interfaces()
  {
    std::vector<hardware_interface::StateInterface> state_interfaces;

    state_interfaces.emplace_back(hardware_interface::StateInterface(
        wheel_lf_.name, hardware_interface::HW_IF_POSITION, &wheel_lf_.pos));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
        wheel_lf_.name, hardware_interface::HW_IF_VELOCITY, &wheel_lf_.vel));

    state_interfaces.emplace_back(hardware_interface::StateInterface(
        wheel_rf_.name, hardware_interface::HW_IF_POSITION, &wheel_rf_.pos));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
        wheel_rf_.name, hardware_interface::HW_IF_VELOCITY, &wheel_rf_.vel));

    state_interfaces.emplace_back(hardware_interface::StateInterface(
        wheel_lb_.name, hardware_interface::HW_IF_POSITION, &wheel_lb_.pos));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
        wheel_lb_.name, hardware_interface::HW_IF_VELOCITY, &wheel_lb_.vel));

    state_interfaces.emplace_back(hardware_interface::StateInterface(
        wheel_rb_.name, hardware_interface::HW_IF_POSITION, &wheel_rb_.pos));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
        wheel_rb_.name, hardware_interface::HW_IF_VELOCITY, &wheel_rb_.vel));

    return state_interfaces;
  }

  std::vector<hardware_interface::CommandInterface> DogdriveArduinoHardware::export_command_interfaces()
  {
    std::vector<hardware_interface::CommandInterface> command_interfaces;

    command_interfaces.emplace_back(hardware_interface::CommandInterface(
        wheel_lf_.name, hardware_interface::HW_IF_VELOCITY, &wheel_lf_.cmd));

    command_interfaces.emplace_back(hardware_interface::CommandInterface(
        wheel_rf_.name, hardware_interface::HW_IF_VELOCITY, &wheel_rf_.cmd));

    command_interfaces.emplace_back(hardware_interface::CommandInterface(
        wheel_lb_.name, hardware_interface::HW_IF_VELOCITY, &wheel_lb_.cmd));

    command_interfaces.emplace_back(hardware_interface::CommandInterface(
        wheel_rb_.name, hardware_interface::HW_IF_VELOCITY, &wheel_rb_.cmd));

    return command_interfaces;
  }

  hardware_interface::CallbackReturn DogdriveArduinoHardware::on_configure(
      const rclcpp_lifecycle::State & /*previous_state*/)
  {
    RCLCPP_INFO(rclcpp::get_logger("DogdriveArduinoHardware"), "Configuring ...please wait...");
    if (comms_.connected() && comms_.disconnect())
    {
      comms_.connect(cfg_.device, cfg_.baud_rate, cfg_.timeout_ms);
      RCLCPP_INFO(rclcpp::get_logger("DogdriveArduinoHardware"), "Successfully configured!");
      return hardware_interface::CallbackReturn::SUCCESS;
    }
    RCLCPP_ERROR(rclcpp::get_logger("DogdriveArduinoHardware"), "Failed to configure!");
    return hardware_interface::CallbackReturn::ERROR;
  }

  hardware_interface::CallbackReturn DogdriveArduinoHardware::on_cleanup(
      const rclcpp_lifecycle::State & /*previous_state*/)
  {
    RCLCPP_INFO(rclcpp::get_logger("DogdriveArduinoHardware"), "Cleaning up ...please wait...");
    if (comms_.connected() && comms_.disconnect())
    {
      RCLCPP_INFO(rclcpp::get_logger("DogdriveArduinoHardware"), "Successfully cleaned up!");
      return hardware_interface::CallbackReturn::SUCCESS;
    }
    RCLCPP_ERROR(rclcpp::get_logger("DogdriveArduinoHardware"), "Failed to clean up!");
    return hardware_interface::CallbackReturn::ERROR;
  }

  hardware_interface::CallbackReturn DogdriveArduinoHardware::on_activate(
      const rclcpp_lifecycle::State & /*previous_state*/)
  {
    RCLCPP_INFO(rclcpp::get_logger("DogdriveArduinoHardware"), "Activating ...please wait...");
    if (!comms_.connected())
    {
      RCLCPP_ERROR(rclcpp::get_logger("DogdriveArduinoHardware"), "Failed to activate!");
      return hardware_interface::CallbackReturn::ERROR;
    }
    if (cfg_.pid_p > 0)
    {
      comms_.set_pid_values(cfg_.pid_p, cfg_.pid_d, cfg_.pid_i, cfg_.pid_o);
    }
    RCLCPP_INFO(rclcpp::get_logger("DogdriveArduinoHardware"), "Successfully activated!");
    return hardware_interface::CallbackReturn::SUCCESS;
  }

  hardware_interface::CallbackReturn DogdriveArduinoHardware::on_deactivate(
      const rclcpp_lifecycle::State & /*previous_state*/)
  {
    RCLCPP_INFO(rclcpp::get_logger("DogdriveArduinoHardware"), "Deactivating ...please wait...");
    RCLCPP_INFO(rclcpp::get_logger("DogdriveArduinoHardware"), "Successfully deactivated!");
    return hardware_interface::CallbackReturn::SUCCESS;
  }

  hardware_interface::return_type DogdriveArduinoHardware::read(
      const rclcpp::Time & /*time*/, const rclcpp::Duration &period)
  {
    if (!comms_.connected())
    {
      RCLCPP_INFO(rclcpp::get_logger("DogdriveArduinoHardware"), "Failed to read!");
      return hardware_interface::return_type::ERROR;
    }

    try
    {
      comms_.read_encoder_values(wheel_lf_.enc, wheel_rf_.enc, wheel_lb_.enc, wheel_rb_.enc);
    }
    catch (const std::exception &e)
    {
      RCLCPP_ERROR(rclcpp::get_logger("DogdriveArduinoHardware"), "Failed to read encoder values: %s", e.what());
      return hardware_interface::return_type::ERROR;
    }

    double delta_seconds = period.seconds();

    double pos_prev = wheel_lf_.pos;
    wheel_lf_.pos = wheel_lf_.calc_enc_angle();
    wheel_lf_.vel = (wheel_lf_.pos - pos_prev) / delta_seconds;

    pos_prev = wheel_rf_.pos;
    wheel_rf_.pos = wheel_rf_.calc_enc_angle();
    wheel_rf_.vel = (wheel_rf_.pos - pos_prev) / delta_seconds;

    pos_prev = wheel_lb_.pos;
    wheel_lb_.pos = wheel_lb_.calc_enc_angle();
    wheel_lb_.vel = (wheel_lb_.pos - pos_prev) / delta_seconds;

    pos_prev = wheel_rb_.pos;
    wheel_rb_.pos = wheel_rb_.calc_enc_angle();
    wheel_rb_.vel = (wheel_rb_.pos - pos_prev) / delta_seconds;

    return hardware_interface::return_type::OK;
  }

  hardware_interface::return_type dogdrive_arduino ::DogdriveArduinoHardware::write(
      const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
  {
    if (!comms_.connected())
    {
      return hardware_interface::return_type::ERROR;
    }

    int motor_lf_counts_per_loop = wheel_lf_.cmd / wheel_lf_.rads_per_count / cfg_.loop_rate;
    int motor_rf_counts_per_loop = wheel_rf_.cmd / wheel_rf_.rads_per_count / cfg_.loop_rate;
    int motor_lb_counts_per_loop = wheel_lb_.cmd / wheel_lb_.rads_per_count / cfg_.loop_rate;
    int motor_rb_counts_per_loop = wheel_rb_.cmd / wheel_rb_.rads_per_count / cfg_.loop_rate;

    try
    {
      comms_.set_motor_values(motor_lf_counts_per_loop, motor_rf_counts_per_loop, motor_lb_counts_per_loop, motor_rb_counts_per_loop);
    }
    catch (const std::exception &e)
    {
      RCLCPP_ERROR(rclcpp::get_logger("DogdriveArduinoHardware"), "Failed to read encoder values: %s", e.what());
      return hardware_interface::return_type::ERROR;
    }

    return hardware_interface::return_type::OK;
  }

} // namespace dogdrive_arduino

#include "pluginlib/class_list_macros.hpp"
PLUGINLIB_EXPORT_CLASS(
    dogdrive_arduino::DogdriveArduinoHardware, hardware_interface::SystemInterface)
