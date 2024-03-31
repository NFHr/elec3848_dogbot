// Copyright 2024
//
// Modifies by Long Liangmao in 2024
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

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "dogbot_drive_controller/dogbot_drive_controller.hpp"
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "lifecycle_msgs/msg/state.hpp"
#include "rclcpp/logging.hpp"
#include "tf2/LinearMath/Quaternion.h"

namespace {
    constexpr auto DEFAULT_COMMAND_TOPIC = "~/cmd_vel";
    constexpr auto DEFAULT_COMMAND_UNSTAMPED_TOPIC = "~/cmd_vel_unstamped";
    constexpr auto DEFAULT_COMMAND_OUT_TOPIC = "~/cmd_vel_out";
    constexpr auto DEFAULT_ODOMETRY_TOPIC = "~/odom";
    constexpr auto DEFAULT_TRANSFORM_TOPIC = "/tf";
} // namespace

namespace dogbot_drive_controller {
    using namespace std::chrono_literals;
    using controller_interface::interface_configuration_type;
    using controller_interface::InterfaceConfiguration;
    using hardware_interface::HW_IF_POSITION;
    using hardware_interface::HW_IF_VELOCITY;
    using lifecycle_msgs::msg::State;

    DogBotDriveController::DogBotDriveController() : controller_interface::ControllerInterface() {}

    const char *DogBotDriveController::feedback_type() const {
        return params_.position_feedback ? HW_IF_POSITION : HW_IF_VELOCITY;
    }

    controller_interface::CallbackReturn DogBotDriveController::on_init() {
        try {
            param_listener_ = std::make_shared<ParamListener>(get_node());
            params_ = param_listener_->get_params();
        }
        catch (const std::exception &e) {
            fprintf(stderr, "Exception thrown during init stage with message: %s \n", e.what());
            return controller_interface::CallbackReturn::ERROR;
        }
        odometry_.init(get_node()->get_clock()->now());
        return controller_interface::CallbackReturn::SUCCESS;
    }

    InterfaceConfiguration DogBotDriveController::command_interface_configuration() const {
        std::vector<std::string> conf_names;
        conf_names.push_back(params_.lf_wheel_name + "/" + HW_IF_VELOCITY);
        conf_names.push_back(params_.rf_wheel_name + "/" + HW_IF_VELOCITY);
        conf_names.push_back(params_.lb_wheel_name + "/" + HW_IF_VELOCITY);
        conf_names.push_back(params_.rb_wheel_name + "/" + HW_IF_VELOCITY);
        return {interface_configuration_type::ALL, conf_names};
    }

    InterfaceConfiguration DogBotDriveController::state_interface_configuration() const {
        std::vector<std::string> conf_names;
        conf_names.push_back(params_.lf_wheel_name + "/" + feedback_type());
        conf_names.push_back(params_.rf_wheel_name + "/" + feedback_type());
        conf_names.push_back(params_.lb_wheel_name + "/" + feedback_type());
        conf_names.push_back(params_.rb_wheel_name + "/" + feedback_type());
        return {interface_configuration_type::ALL, conf_names};
    }

    controller_interface::return_type DogBotDriveController::update(
            const rclcpp::Time &time, const rclcpp::Duration &period) {
        auto logger = get_node()->get_logger();
        if (get_state().id() == State::PRIMARY_STATE_INACTIVE) {
            if (!is_halted) {
                halt();
                is_halted = true;
            }
            return controller_interface::return_type::OK;
        }

        std::shared_ptr<Twist> last_command_msg;
        received_velocity_msg_ptr_.get(last_command_msg);

        if (last_command_msg == nullptr) {
            RCLCPP_WARN(logger, "Velocity message received was a nullptr.");
            return controller_interface::return_type::ERROR;
        }

        const auto age_of_last_command = time - last_command_msg->header.stamp;
        // Brake if cmd_vel has timeout, override the stored command
        if (age_of_last_command > cmd_vel_timeout_) {
            last_command_msg->twist.linear.x = 0.0;
            last_command_msg->twist.linear.y = 0.0;
            last_command_msg->twist.angular.z = 0.0;
        }

        // command may be limited further by SpeedLimit,
        // without affecting the stored twist command
        Twist command = *last_command_msg;
        double &linear_command_x = command.twist.linear.x;
        double &linear_command_y = command.twist.linear.y;
        double &angular_command = command.twist.angular.z;

        previous_update_timestamp_ = time;

        // Apply (possibly new) multipliers:
        const double wheel_separation_x = params_.wheel_separation_x;
        const double wheel_separation_y = params_.wheel_separation_y;
        const double wheel_separation_k = (wheel_separation_x + wheel_separation_y) / 2.0;

        const double lf_wheel_radius = params_.lf_wheel_radius_multiplier * params_.wheel_radius;
        const double rf_wheel_radius = params_.rf_wheel_radius_multiplier * params_.wheel_radius;
        const double lb_wheel_radius = params_.lb_wheel_radius_multiplier * params_.wheel_radius;
        const double rb_wheel_radius = params_.rb_wheel_radius_multiplier * params_.wheel_radius;

        const double lf_feedback = registered_handles_.at(params_.lf_wheel_name).feedback.get().get_value();
        const double rf_feedback = registered_handles_.at(params_.rf_wheel_name).feedback.get().get_value();
        const double lb_feedback = registered_handles_.at(params_.lb_wheel_name).feedback.get().get_value();
        const double rb_feedback = registered_handles_.at(params_.rb_wheel_name).feedback.get().get_value();

        if (std::isnan(lf_feedback) || std::isnan(rf_feedback) || std::isnan(lb_feedback) || std::isnan(rb_feedback)) {
            RCLCPP_ERROR(
                    logger, "The wheel %s is invalid ", feedback_type());
            return controller_interface::return_type::ERROR;
        }

        if (params_.position_feedback) {
            odometry_.update(lf_feedback * lf_wheel_radius,
                             rf_feedback * rf_wheel_radius,
                             lb_feedback * lb_wheel_radius,
                             rb_feedback * rb_wheel_radius, time);
        } else {
            odometry_.updateFromVelocity(
                    lf_feedback * lf_wheel_radius * period.seconds(),
                    rf_feedback * rf_wheel_radius * period.seconds(),
                    lb_feedback * lb_wheel_radius * period.seconds(),
                    rb_feedback * rb_wheel_radius * period.seconds(), time);
        }

        tf2::Quaternion orientation;
        orientation.setRPY(0.0, 0.0, odometry_.getHeading());

        bool should_publish = false;
        try {
            if (previous_publish_timestamp_ + publish_period_ < time) {
                previous_publish_timestamp_ += publish_period_;
                should_publish = true;
            }
        }
        catch (const std::runtime_error &) {
            // Handle exceptions when the time source changes and initialize publish timestamp
            previous_publish_timestamp_ = time;
            should_publish = true;
        }

        if (should_publish) {
            if (realtime_odometry_publisher_->trylock()) {
                auto &odometry_message = realtime_odometry_publisher_->msg_;
                odometry_message.header.stamp = time;
                odometry_message.pose.pose.position.x = odometry_.getX();
                odometry_message.pose.pose.position.y = odometry_.getY();
                odometry_message.pose.pose.orientation.x = orientation.x();
                odometry_message.pose.pose.orientation.y = orientation.y();
                odometry_message.pose.pose.orientation.z = orientation.z();
                odometry_message.pose.pose.orientation.w = orientation.w();
                odometry_message.twist.twist.linear.x = odometry_.getLinearX();
                odometry_message.twist.twist.linear.y = odometry_.getLinearY();
                odometry_message.twist.twist.angular.z = odometry_.getAngular();
                realtime_odometry_publisher_->unlockAndPublish();
            }

            if (params_.enable_odom_tf && realtime_odometry_transform_publisher_->trylock()) {
                auto &transform = realtime_odometry_transform_publisher_->msg_.transforms.front();
                transform.header.stamp = time;
                transform.transform.translation.x = odometry_.getX();
                transform.transform.translation.y = odometry_.getY();
                transform.transform.rotation.x = orientation.x();
                transform.transform.rotation.y = orientation.y();
                transform.transform.rotation.z = orientation.z();
                transform.transform.rotation.w = orientation.w();
                realtime_odometry_transform_publisher_->unlockAndPublish();
            }
        }

        auto &last_command = previous_commands_.back().twist;
        auto &second_to_last_command = previous_commands_.front().twist;
        limiter_linear_x_.limit(linear_command_x, last_command.linear.x, second_to_last_command.linear.x,
                                period.seconds());
        limiter_linear_y_.limit(linear_command_y, last_command.linear.y, second_to_last_command.linear.y,
                                period.seconds());
        limiter_angular_.limit(angular_command, last_command.angular.z, second_to_last_command.angular.z,
                               period.seconds());

        previous_commands_.pop();
        previous_commands_.emplace(command);

        // Publish limited velocity
        if (publish_limited_velocity_ && realtime_limited_velocity_publisher_->trylock()) {
            auto &limited_velocity_command = realtime_limited_velocity_publisher_->msg_;
            limited_velocity_command.header.stamp = time;
            limited_velocity_command.twist = command.twist;
            realtime_limited_velocity_publisher_->unlockAndPublish();
        }

        // Compute wheels angular velocities (in rad/s):
        const double angular_velocity_lf =
                (linear_command_x - linear_command_y + angular_command * wheel_separation_k) / lf_wheel_radius;
        const double angular_velocity_rf =
                (linear_command_x + linear_command_y - angular_command * wheel_separation_k) / rf_wheel_radius;
        const double angular_velocity_lb =
                (linear_command_x + linear_command_y + angular_command * wheel_separation_k) / lb_wheel_radius;
        const double angular_velocity_rb =
                (linear_command_x - linear_command_y - angular_command * wheel_separation_k) / rb_wheel_radius;

        // Set wheels angular velocities:
        registered_handles_.at(params_.lf_wheel_name).velocity.get().set_value(angular_velocity_lf);
        registered_handles_.at(params_.rf_wheel_name).velocity.get().set_value(angular_velocity_rf);
        registered_handles_.at(params_.lb_wheel_name).velocity.get().set_value(angular_velocity_lb);
        registered_handles_.at(params_.rb_wheel_name).velocity.get().set_value(angular_velocity_rb);

        return controller_interface::return_type::OK;
    }

    controller_interface::CallbackReturn DogBotDriveController::on_configure(const rclcpp_lifecycle::State &) {
        auto logger = get_node()->get_logger();

        // update parameters if they have changed
        if (param_listener_->is_old(params_)) {
            params_ = param_listener_->get_params();
            RCLCPP_INFO(logger, "Parameters were updated");
        }

        const double wheel_separation_x = params_.wheel_separation_x;
        const double wheel_separation_y = params_.wheel_separation_y;

        const double lf_wheel_radius = params_.lf_wheel_radius_multiplier * params_.wheel_radius;
        const double rf_wheel_radius = params_.rf_wheel_radius_multiplier * params_.wheel_radius;
        const double lb_wheel_radius = params_.lb_wheel_radius_multiplier * params_.wheel_radius;
        const double rb_wheel_radius = params_.rb_wheel_radius_multiplier * params_.wheel_radius;

        odometry_.setWheelParams(wheel_separation_x, wheel_separation_y, lf_wheel_radius, rf_wheel_radius,
                                 lb_wheel_radius, rb_wheel_radius);
        odometry_.setVelocityRollingWindowSize(params_.velocity_rolling_window_size);

        cmd_vel_timeout_ = std::chrono::milliseconds{static_cast<int>(params_.cmd_vel_timeout * 1000.0)};
        publish_limited_velocity_ = params_.publish_limited_velocity;
        use_stamped_vel_ = params_.use_stamped_vel;

        limiter_linear_x_ = SpeedLimiter(
                params_.linear.x.has_velocity_limits, params_.linear.x.has_acceleration_limits,
                params_.linear.x.has_jerk_limits, params_.linear.x.min_velocity, params_.linear.x.max_velocity,
                params_.linear.x.min_acceleration, params_.linear.x.max_acceleration, params_.linear.x.min_jerk,
                params_.linear.x.max_jerk);

        limiter_linear_y_ = SpeedLimiter(
                params_.linear.y.has_velocity_limits, params_.linear.y.has_acceleration_limits,
                params_.linear.y.has_jerk_limits, params_.linear.y.min_velocity, params_.linear.y.max_velocity,
                params_.linear.y.min_acceleration, params_.linear.y.max_acceleration, params_.linear.y.min_jerk,
                params_.linear.y.max_jerk);

        limiter_angular_ = SpeedLimiter(
                params_.angular.z.has_velocity_limits, params_.angular.z.has_acceleration_limits,
                params_.angular.z.has_jerk_limits, params_.angular.z.min_velocity,
                params_.angular.z.max_velocity, params_.angular.z.min_acceleration,
                params_.angular.z.max_acceleration, params_.angular.z.min_jerk, params_.angular.z.max_jerk);
        reset();

//        if (!reset()) {
//            return controller_interface::CallbackReturn::ERROR;
//        }

        if (publish_limited_velocity_) {
            limited_velocity_publisher_ = get_node()->create_publisher<Twist>(DEFAULT_COMMAND_OUT_TOPIC,
                                                                              rclcpp::SystemDefaultsQoS());
            realtime_limited_velocity_publisher_ = std::make_shared<realtime_tools::RealtimePublisher<Twist>>(
                    limited_velocity_publisher_);
        }

        const Twist empty_twist;
        received_velocity_msg_ptr_.set(std::make_shared<Twist>(empty_twist));

        // Fill last two commands with default constructed commands
        previous_commands_.emplace(empty_twist);
        previous_commands_.emplace(empty_twist);

        // initialize command subscriber
        if (use_stamped_vel_) {
            velocity_command_subscriber_ = get_node()->create_subscription<Twist>(
                    DEFAULT_COMMAND_TOPIC, rclcpp::SystemDefaultsQoS(),
                    [this](const std::shared_ptr<Twist> msg) -> void {
                        if (!subscriber_is_active_) {
                            RCLCPP_WARN(
                                    get_node()->get_logger(), "Can't accept new commands. subscriber is inactive");
                            return;
                        }
                        if ((msg->header.stamp.sec == 0) && (msg->header.stamp.nanosec == 0)) {
                            RCLCPP_WARN_ONCE(
                                    get_node()->get_logger(),
                                    "Received TwistStamped with zero timestamp, setting it to current "
                                    "time, this message will only be shown once");
                            msg->header.stamp = get_node()->get_clock()->now();
                        }
                        received_velocity_msg_ptr_.set(std::move(msg));
                    });
        } else {
            velocity_command_unstamped_subscriber_ =
                    get_node()->create_subscription<geometry_msgs::msg::Twist>(
                            DEFAULT_COMMAND_UNSTAMPED_TOPIC, rclcpp::SystemDefaultsQoS(),
                            [this](const std::shared_ptr<geometry_msgs::msg::Twist> msg) -> void {
                                if (!subscriber_is_active_) {
                                    RCLCPP_WARN(
                                            get_node()->get_logger(),
                                            "Can't accept new commands. subscriber is inactive");
                                    return;
                                }
                                std::shared_ptr<Twist> twist_stamped;
                                received_velocity_msg_ptr_.get(twist_stamped);
                                twist_stamped->twist = *msg;
                                twist_stamped->header.stamp = get_node()->get_clock()->now();
                            });
        }

        // initialize odometry publisher and messasge
        odometry_publisher_ = get_node()->create_publisher<nav_msgs::msg::Odometry>(DEFAULT_ODOMETRY_TOPIC,
                                                                                    rclcpp::SystemDefaultsQoS());
        realtime_odometry_publisher_ = std::make_shared<realtime_tools::RealtimePublisher<nav_msgs::msg::Odometry>>(
                odometry_publisher_);

        // Append the tf prefix if there is one
        std::string tf_prefix;
        if (params_.tf_frame_prefix_enable) {
            if (!params_.tf_frame_prefix.empty()) {
                tf_prefix = params_.tf_frame_prefix;
            } else {
                tf_prefix = std::string(get_node()->get_namespace());
            }
            if (tf_prefix == "/") {
                tf_prefix = "";
            } else {
                tf_prefix = tf_prefix + "/";
            }
        }

        const auto odom_frame_id = tf_prefix + params_.odom_frame_id;
        const auto base_frame_id = tf_prefix + params_.base_frame_id;

        auto &odometry_message = realtime_odometry_publisher_->msg_;
        odometry_message.header.frame_id = odom_frame_id;
        odometry_message.child_frame_id = base_frame_id;

        // limit the publication on the topics /odom and /tf
        publish_rate_ = params_.publish_rate;
        publish_period_ = rclcpp::Duration::from_seconds(1.0 / publish_rate_);

        // initialize odom values zeros
        odometry_message.twist = geometry_msgs::msg::TwistWithCovariance(
                rosidl_runtime_cpp::MessageInitialization::ALL);

        constexpr size_t NUM_DIMENSIONS = 6;
        for (size_t index = 0; index < 6; ++index) {
            // 0, 7, 14, 21, 28, 35
            const size_t diagonal_index = NUM_DIMENSIONS * index + index;
            odometry_message.pose.covariance[diagonal_index] = params_.pose_covariance_diagonal[index];
            odometry_message.twist.covariance[diagonal_index] = params_.twist_covariance_diagonal[index];
        }

        // initialize transform publisher and message
        odometry_transform_publisher_ = get_node()->create_publisher<tf2_msgs::msg::TFMessage>(DEFAULT_TRANSFORM_TOPIC,
                                                                                               rclcpp::SystemDefaultsQoS());
        realtime_odometry_transform_publisher_ = std::make_shared<realtime_tools::RealtimePublisher<tf2_msgs::msg::TFMessage>>(
                odometry_transform_publisher_);

        // keeping track of odom and base_link transforms only
        auto &odometry_transform_message = realtime_odometry_transform_publisher_->msg_;
        odometry_transform_message.transforms.resize(1);
        odometry_transform_message.transforms.front().header.frame_id = odom_frame_id;
        odometry_transform_message.transforms.front().child_frame_id = base_frame_id;

        previous_update_timestamp_ = get_node()->get_clock()->now();
        return controller_interface::CallbackReturn::SUCCESS;
    }

    controller_interface::CallbackReturn DogBotDriveController::on_activate(const rclcpp_lifecycle::State &) {
        const auto lf_result = configure_wheel(params_.lf_wheel_name);
        const auto rf_result = configure_wheel(params_.rf_wheel_name);
        const auto lb_result = configure_wheel(params_.lb_wheel_name);
        const auto rb_result = configure_wheel(params_.rb_wheel_name);

        if (
                lf_result == controller_interface::CallbackReturn::ERROR ||
                rf_result == controller_interface::CallbackReturn::ERROR ||
                lb_result == controller_interface::CallbackReturn::ERROR ||
                rb_result == controller_interface::CallbackReturn::ERROR) {
            return controller_interface::CallbackReturn::ERROR;
        }

        is_halted = false;
        subscriber_is_active_ = true;

        return controller_interface::CallbackReturn::SUCCESS;
    }

    controller_interface::CallbackReturn DogBotDriveController::on_deactivate(const rclcpp_lifecycle::State &) {
        subscriber_is_active_ = false;
        if (!is_halted) {
            halt();
            is_halted = true;
        }
        registered_handles_.clear();
        return controller_interface::CallbackReturn::SUCCESS;
    }

    controller_interface::CallbackReturn DogBotDriveController::on_cleanup(const rclcpp_lifecycle::State &) {
        reset();
//        if (!reset()) {
//            return controller_interface::CallbackReturn::ERROR;
//        }
        received_velocity_msg_ptr_.set(std::make_shared<Twist>());
        return controller_interface::CallbackReturn::SUCCESS;
    }

    controller_interface::CallbackReturn DogBotDriveController::on_error(const rclcpp_lifecycle::State &) {
        reset();
/*        if (!reset()) {
            return controller_interface::CallbackReturn::ERROR;
        }*/
        return controller_interface::CallbackReturn::SUCCESS;
    }

    void DogBotDriveController::reset() {
        odometry_.resetOdometry();

        std::queue<Twist> empty;
        std::swap(previous_commands_, empty);

        registered_handles_.clear();

        subscriber_is_active_ = false;
        velocity_command_subscriber_.reset();
        velocity_command_unstamped_subscriber_.reset();

        received_velocity_msg_ptr_.set(nullptr);
        is_halted = false;
    }

    controller_interface::CallbackReturn DogBotDriveController::on_shutdown(const rclcpp_lifecycle::State &) {
        return controller_interface::CallbackReturn::SUCCESS;
    }

    void DogBotDriveController::halt() {
        for (const auto &[name, wheel_handle]: registered_handles_) {
            wheel_handle.velocity.get().set_value(0.0);
        }
    }

    controller_interface::CallbackReturn DogBotDriveController::configure_wheel(const std::string &wheel_name) {
        auto logger = get_node()->get_logger();

        if (wheel_name.empty()) {
            return controller_interface::CallbackReturn::ERROR;
        }

        // register handle
        const auto interface_name = feedback_type();
        const auto state_handle = std::find_if(
                state_interfaces_.cbegin(), state_interfaces_.cend(),
                [&wheel_name, &interface_name](const auto &interface) {
                    return interface.get_prefix_name() == wheel_name &&
                           interface.get_interface_name() == interface_name;
                });

        if (state_handle == state_interfaces_.cend()) {
            RCLCPP_ERROR(logger, "Unable to obtain joint state handle for %s", wheel_name.c_str());
            return controller_interface::CallbackReturn::ERROR;
        }

        const auto command_handle = std::find_if(
                command_interfaces_.begin(), command_interfaces_.end(),
                [&wheel_name](const auto &interface) {
                    return interface.get_prefix_name() == wheel_name &&
                           interface.get_interface_name() == HW_IF_VELOCITY;
                });

        if (command_handle == command_interfaces_.end()) {
            RCLCPP_ERROR(logger, "Unable to obtain joint command handle for %s", wheel_name.c_str());
            return controller_interface::CallbackReturn::ERROR;
        }

        registered_handles_.insert({wheel_name, WheelHandle{std::ref(*state_handle), std::ref(*command_handle)}});

        return controller_interface::CallbackReturn::SUCCESS;
    }
} // namespace dogbot_drive_controller

#include "class_loader/register_macro.hpp"

CLASS_LOADER_REGISTER_CLASS(
        dogbot_drive_controller::DogBotDriveController, controller_interface::ControllerInterface)
