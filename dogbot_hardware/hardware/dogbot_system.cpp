// Copyright 2024 Long Liangmao
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

#include "dogbot_hardware/dogbot_system.hpp"

#include <vector>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

namespace dogbot_hardware {
    hardware_interface::CallbackReturn DogBotSystemHardware::on_init(
            const hardware_interface::HardwareInfo &info) {
        if (
                hardware_interface::SystemInterface::on_init(info) !=
                hardware_interface::CallbackReturn::SUCCESS) {
            return hardware_interface::CallbackReturn::ERROR;
        }

        RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Initializing... please wait...");

        cfg_.device = info_.hardware_parameters["device"];
        cfg_.baud_rate = std::stoi(info_.hardware_parameters["baud_rate"]);
        cfg_.timeout_ms = std::stoi(info_.hardware_parameters["timeout_ms"]);

        wheel_lf_.name = info_.hardware_parameters["lf_wheel_name"];
        wheel_rf_.name = info_.hardware_parameters["rf_wheel_name"];
        wheel_lb_.name = info_.hardware_parameters["lb_wheel_name"];
        wheel_rb_.name = info_.hardware_parameters["rb_wheel_name"];

        for (const hardware_interface::ComponentInfo &joint: info_.joints) {
            // DogBotSystem has exactly two states and one command interface on each joint (wheel)
            if (joint.command_interfaces.size() != 1) {
                RCLCPP_FATAL(
                        rclcpp::get_logger("DogBotSystemHardware"),
                        "Joint '%s' has %zu command interfaces found. 1 expected.", joint.name.c_str(),
                        joint.command_interfaces.size());
                return hardware_interface::CallbackReturn::ERROR;
            }

            if (joint.command_interfaces[0].name != hardware_interface::HW_IF_VELOCITY) {
                RCLCPP_FATAL(
                        rclcpp::get_logger("DogBotSystemHardware"),
                        "Joint '%s' have %s command interfaces found. '%s' expected.", joint.name.c_str(),
                        joint.command_interfaces[0].name.c_str(), hardware_interface::HW_IF_VELOCITY);
                return hardware_interface::CallbackReturn::ERROR;
            }

            if (joint.state_interfaces.size() != 2) {
                RCLCPP_FATAL(
                        rclcpp::get_logger("DogBotSystemHardware"),
                        "Joint '%s' has %zu state interface. 2 expected.", joint.name.c_str(),
                        joint.state_interfaces.size());
                return hardware_interface::CallbackReturn::ERROR;
            }

            if (joint.state_interfaces[0].name != hardware_interface::HW_IF_POSITION) {
                RCLCPP_FATAL(
                        rclcpp::get_logger("DogBotSystemHardware"),
                        "Joint '%s' have '%s' as first state interface. '%s' expected.", joint.name.c_str(),
                        joint.state_interfaces[0].name.c_str(), hardware_interface::HW_IF_POSITION);
                return hardware_interface::CallbackReturn::ERROR;
            }

            if (joint.state_interfaces[1].name != hardware_interface::HW_IF_VELOCITY) {
                RCLCPP_FATAL(
                        rclcpp::get_logger("DogBotSystemHardware"),
                        "Joint '%s' have '%s' as second state interface. '%s' expected.", joint.name.c_str(),
                        joint.state_interfaces[1].name.c_str(), hardware_interface::HW_IF_VELOCITY);
                return hardware_interface::CallbackReturn::ERROR;
            }
        }

        return hardware_interface::CallbackReturn::SUCCESS;
    }

    std::vector<hardware_interface::StateInterface> DogBotSystemHardware::export_state_interfaces() {
        RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Exporting State Interfaces... please wait...");

        std::vector<hardware_interface::StateInterface> state_interfaces;
        state_interfaces.emplace_back(wheel_lf_.name, hardware_interface::HW_IF_POSITION, &wheel_lf_.pos);
        state_interfaces.emplace_back(
                wheel_lf_.name, hardware_interface::HW_IF_VELOCITY, &wheel_lf_.vel);

        state_interfaces.emplace_back(
                wheel_rf_.name, hardware_interface::HW_IF_POSITION, &wheel_rf_.pos);
        state_interfaces.emplace_back(
                wheel_rf_.name, hardware_interface::HW_IF_VELOCITY, &wheel_rf_.vel);

        state_interfaces.emplace_back(
                wheel_lb_.name, hardware_interface::HW_IF_POSITION, &wheel_lb_.pos);
        state_interfaces.emplace_back(
                wheel_lb_.name, hardware_interface::HW_IF_VELOCITY, &wheel_lb_.vel);

        state_interfaces.emplace_back(
                wheel_rb_.name, hardware_interface::HW_IF_POSITION, &wheel_rb_.pos);
        state_interfaces.emplace_back(
                wheel_rb_.name, hardware_interface::HW_IF_VELOCITY, &wheel_rb_.vel);

        return state_interfaces;
    }

    std::vector<hardware_interface::CommandInterface> DogBotSystemHardware::export_command_interfaces() {
        RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Exporting Command Interfaces... please wait...");

        std::vector<hardware_interface::CommandInterface> command_interfaces;

        command_interfaces.emplace_back(
                wheel_lf_.name, hardware_interface::HW_IF_VELOCITY, &wheel_lf_.cmd);

        command_interfaces.emplace_back(
                wheel_rf_.name, hardware_interface::HW_IF_VELOCITY, &wheel_rf_.cmd);

        command_interfaces.emplace_back(
                wheel_lb_.name, hardware_interface::HW_IF_VELOCITY, &wheel_lb_.cmd);

        command_interfaces.emplace_back(
                wheel_rb_.name, hardware_interface::HW_IF_VELOCITY, &wheel_rb_.cmd);

        return command_interfaces;
    }

    hardware_interface::CallbackReturn DogBotSystemHardware::on_configure(
            const rclcpp_lifecycle::State & /*previous_state*/) {
        RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Configuring... please wait...");
        if (comms_.connected()) {
            RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Reconnecting...");
            comms_.disconnect();
        }
        if (comms_.connect(cfg_.device, cfg_.baud_rate, cfg_.timeout_ms)) {
            RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Successfully configured!");
            return hardware_interface::CallbackReturn::SUCCESS;
        }
        RCLCPP_ERROR(rclcpp::get_logger("DogBotSystemHardware"), "Failed to Configure!");
        return hardware_interface::CallbackReturn::ERROR;
    }

    hardware_interface::CallbackReturn DogBotSystemHardware::on_cleanup(
            const rclcpp_lifecycle::State & /*previous_state*/) {
        RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Cleaning... please wait...");
        if (comms_.connected() && comms_.disconnect()) {
            RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Successfully cleaned up!");
            return hardware_interface::CallbackReturn::SUCCESS;
        }
        RCLCPP_ERROR(rclcpp::get_logger("DogBotSystemHardware"), "Failed to clean up!");
        return hardware_interface::CallbackReturn::ERROR;
    }

    hardware_interface::CallbackReturn DogBotSystemHardware::on_activate(
            const rclcpp_lifecycle::State & /*previous_state*/) {
        RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Activating ...please wait...");
        if (!comms_.connected()) {
            RCLCPP_ERROR(rclcpp::get_logger("DogBotSystemHardware"), "Failed to activate!");
            return hardware_interface::CallbackReturn::ERROR;
        }
        RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Successfully activated!");

        return hardware_interface::CallbackReturn::SUCCESS;
    }

    hardware_interface::CallbackReturn DogBotSystemHardware::on_deactivate(
            const rclcpp_lifecycle::State & /*previous_state*/) {
        RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Deactivating ...please wait...");
        RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Successfully deactivated!");

        return hardware_interface::CallbackReturn::SUCCESS;
    }

    hardware_interface::return_type DogBotSystemHardware::read(
            const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/) {
        if (!comms_.connected()) {
            RCLCPP_INFO(rclcpp::get_logger("DogBotSystemHardware"), "Failed to read!");
            return hardware_interface::return_type::ERROR;
        }
        try {
            comms_.read_feedback("<V>", wheel_lf_.vel, wheel_rf_.vel, wheel_lb_.vel, wheel_rb_.vel);
            comms_.read_feedback("<P>", wheel_lf_.pos, wheel_rf_.pos, wheel_lb_.pos, wheel_rb_.pos);
        }
        catch (const std::exception &e) {
            RCLCPP_ERROR(rclcpp::get_logger("DogBotSystemHardware"), "Failed to read feedback data: %s", e.what());
            return hardware_interface::return_type::ERROR;
        }
        return hardware_interface::return_type::OK;
    }

    hardware_interface::return_type dogbot_hardware::DogBotSystemHardware::write(
            const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/) {
        if (!comms_.connected()) {
            return hardware_interface::return_type::ERROR;
        }

        double motor_lf_velocity = wheel_lf_.cmd;
        double motor_rf_velocity = wheel_rf_.cmd;
        double motor_lb_velocity = wheel_lb_.cmd;
        double motor_rb_velocity = wheel_rb_.cmd;

        try {
            comms_.set_angular_velocity(motor_lf_velocity, motor_rf_velocity, motor_lb_velocity, motor_rb_velocity);
        }
        catch (const std::exception &e) {
            RCLCPP_ERROR(rclcpp::get_logger("DogBotSystemHardware"), "Failed to set velocity values: %s", e.what());
            return hardware_interface::return_type::ERROR;
        }
        return hardware_interface::return_type::OK;
    }

} // namespace dogbot_hardware

#include "pluginlib/class_list_macros.hpp"

PLUGINLIB_EXPORT_CLASS(
        dogbot_hardware::DogBotSystemHardware, hardware_interface::SystemInterface)
