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

#ifndef DOGBOT_HARDWARE_DOGBOT_SYSTEM_HPP_
#define DOGBOT_HARDWARE_DOGBOT_SYSTEM_HPP_

#include <memory>
#include <string>
#include <vector>

#include "dogbot_hardware/visibility_control.h"
#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/clock.hpp"
#include "rclcpp/duration.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp/time.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "rclcpp_lifecycle/state.hpp"

#include "dogbot_hardware/serial_comms.hpp"

namespace dogbot_hardware {
    class DogBotSystemHardware : public hardware_interface::SystemInterface {

        struct Config {
            std::string device;
            int baud_rate = 0;
            int timeout_ms = 1000;
        };

        struct Wheel {
            std::string name;
            double cmd = 0;
            double pos = 0;
            double vel = 0;
        };

    public:
        RCLCPP_SHARED_PTR_DEFINITIONS(DogBotSystemHardware);

        DOGBOT_HARDWARE_PUBLIC
        hardware_interface::CallbackReturn on_init(
                const hardware_interface::HardwareInfo &info) override;

        DOGBOT_HARDWARE_PUBLIC
        std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

        DOGBOT_HARDWARE_PUBLIC
        std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

        DOGBOT_HARDWARE_PUBLIC
        hardware_interface::CallbackReturn on_configure(
                const rclcpp_lifecycle::State &previous_state) override;

        DOGBOT_HARDWARE_PUBLIC
        hardware_interface::CallbackReturn on_cleanup(
                const rclcpp_lifecycle::State &previous_state) override;

        DOGBOT_HARDWARE_PUBLIC
        hardware_interface::CallbackReturn on_activate(
                const rclcpp_lifecycle::State &previous_state) override;

        DOGBOT_HARDWARE_PUBLIC
        hardware_interface::CallbackReturn on_deactivate(
                const rclcpp_lifecycle::State &previous_state) override;

        DOGBOT_HARDWARE_PUBLIC
        hardware_interface::return_type read(
                const rclcpp::Time &time, const rclcpp::Duration &period) override;

        DOGBOT_HARDWARE_PUBLIC
        hardware_interface::return_type write(
                const rclcpp::Time &time, const rclcpp::Duration &period) override;

    private:
        SerialComms comms_;
        Config cfg_;
        Wheel wheel_lf_;
        Wheel wheel_rf_;
        Wheel wheel_lb_;
        Wheel wheel_rb_;
    };

} // namespace dogbot_hardware

#endif // DOGBOT_HARDWARE_DOGBOT_SYSTEM_HPP_
