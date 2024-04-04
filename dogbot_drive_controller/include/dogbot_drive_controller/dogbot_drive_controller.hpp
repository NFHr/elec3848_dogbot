// Copyright 2020 PAL Robotics S.L.
//
// Modified by Long Liangmao in 2024
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

/*
 * Author: Bence Magyar, Enrique Fernández, Manuel Meraz
 */

#ifndef DOGBOT_DRIVE_CONTROLLER_DOGBOT_DRIVE_CONTROLLER_HPP_
#define DOGBOT_DRIVE_CONTROLLER_DOGBOT_DRIVE_CONTROLLER_HPP_

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "controller_interface/controller_interface.hpp"
#include "dogbot_drive_controller/odometry.hpp"
#include "dogbot_drive_controller/visibility_control.h"
#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "hardware_interface/handle.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "odometry.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/state.hpp"
#include "realtime_tools/realtime_box.h"
#include "realtime_tools/realtime_buffer.h"
#include "realtime_tools/realtime_publisher.h"
#include "tf2_msgs/msg/tf_message.hpp"

// auto-generated by generate_parameter_library
#include "dogbot_drive_controller_parameters.hpp"

namespace dogbot_drive_controller {
    class DogBotDriveController : public controller_interface::ControllerInterface {
        using Twist = geometry_msgs::msg::TwistStamped;

    public:
        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        DogBotDriveController();

        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        controller_interface::InterfaceConfiguration command_interface_configuration() const override;

        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        controller_interface::InterfaceConfiguration state_interface_configuration() const override;

        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        controller_interface::return_type update(
                const rclcpp::Time &time, const rclcpp::Duration &period) override;

        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        controller_interface::CallbackReturn on_init() override;

        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        controller_interface::CallbackReturn on_configure(
                const rclcpp_lifecycle::State &previous_state) override;

        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        controller_interface::CallbackReturn on_activate(
                const rclcpp_lifecycle::State &previous_state) override;

        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        controller_interface::CallbackReturn on_deactivate(
                const rclcpp_lifecycle::State &previous_state) override;

        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        controller_interface::CallbackReturn on_cleanup(
                const rclcpp_lifecycle::State &previous_state) override;

        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        controller_interface::CallbackReturn on_error(
                const rclcpp_lifecycle::State &previous_state) override;

        DOGBOT_DRIVE_CONTROLLER_PUBLIC
        controller_interface::CallbackReturn on_shutdown(
                const rclcpp_lifecycle::State &previous_state) override;

    protected:
        struct WheelHandle {
            std::reference_wrapper<const hardware_interface::LoanedStateInterface> feedback;
            std::reference_wrapper<hardware_interface::LoanedCommandInterface> velocity;
        };

        controller_interface::CallbackReturn configure_wheel(const std::string &wheel_name);

        std::map<std::string, WheelHandle> registered_handles_;

        // Parameters from ROS
        std::shared_ptr<ParamListener> param_listener_;
        Params params_;

        Odometry odometry_;

        // Timeout to consider cmd_vel commands old
        std::chrono::milliseconds cmd_vel_timeout_{500};

        std::shared_ptr<rclcpp::Publisher<nav_msgs::msg::Odometry>> odometry_publisher_ = nullptr;
        std::shared_ptr<realtime_tools::RealtimePublisher<nav_msgs::msg::Odometry>>
                realtime_odometry_publisher_ = nullptr;

        std::shared_ptr<rclcpp::Publisher<tf2_msgs::msg::TFMessage>> odometry_transform_publisher_ =
                nullptr;
        std::shared_ptr<realtime_tools::RealtimePublisher<tf2_msgs::msg::TFMessage>>
                realtime_odometry_transform_publisher_ = nullptr;

        bool subscriber_is_active_ = false;
        rclcpp::Subscription<Twist>::SharedPtr velocity_command_subscriber_ = nullptr;
        
        realtime_tools::RealtimeBox<std::shared_ptr<Twist>> received_velocity_msg_ptr_{nullptr};

        rclcpp::Time previous_update_timestamp_{0};

        // publish rate limiter
        double publish_rate_ = 50.0;
        rclcpp::Duration publish_period_ = rclcpp::Duration::from_nanoseconds(0);
        rclcpp::Time previous_publish_timestamp_{0, 0, RCL_CLOCK_UNINITIALIZED};

        bool is_halted_ = false;

        void reset();

        void halt();
    };
} // namespace dogbot_drive_controller
#endif // DOGBOT_DRIVE_CONTROLLER_DOGBOT_DRIVE_CONTROLLER_HPP_
