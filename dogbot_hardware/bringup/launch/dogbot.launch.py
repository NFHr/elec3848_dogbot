# Copyright 2020 ros2_control Development Team
#
# Modified by Long Liangmao in 2024
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    RegisterEventHandler,
    IncludeLaunchDescription,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
from launch.substitutions import (
    Command,
    FindExecutable,
    PathJoinSubstitution,
    LaunchConfiguration,
)
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    # Declare arguments
    declared_arguments = []
    declared_arguments.append(
        DeclareLaunchArgument(
            "gui",
            default_value="true",
            description="Start RViz2 automatically with this launch file.",
        )
    )

    # Initialize Arguments
    gui = LaunchConfiguration("gui")

    # Get URDF via xacro
    robot_description_content = Command(
        [
            PathJoinSubstitution([FindExecutable(name="xacro")]),
            " ",
            PathJoinSubstitution(
                [FindPackageShare("dogbot_hardware"), "urdf", "dogbot.urdf.xacro"]
            ),
            " ",
        ]
    )
    robot_description = {"robot_description": robot_description_content}

    robot_controllers = PathJoinSubstitution(
        [
            FindPackageShare("dogbot_hardware"),
            "config",
            "dogbot_controllers.yaml",
        ]
    )

    rviz_config_file = PathJoinSubstitution(
        [FindPackageShare("dogbot_description"), "dogbot/rviz", "dogbot.rviz"]
    )

    control_node = Node(
        package="controller_manager",
        executable="ros2_control_node",
        parameters=[robot_controllers],
        output="both",
        remappings=[("~/robot_description", "/robot_description")],
    )

    robot_state_pub_node = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="both",
        parameters=[robot_description],
    )

    rviz_node = Node(
        package="rviz2",
        executable="rviz2",
        name="rviz2",
        output="log",
        arguments=["-d", rviz_config_file],
        condition=IfCondition(gui),
    )

    joint_state_broadcaster_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "joint_state_broadcaster",
            "--controller-manager",
            "/controller_manager",
        ],
    )

    robot_drive_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "dogbot_base_controller",
            "--controller-manager",
            "/controller_manager",
        ],
    )

    dogbot_servo_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=[
            "forward_position_controller",
            "--controller-manager",
            "/controller_manager",
        ],
    )

    # Delay rviz start after `joint_state_broadcaster`
    delay_rviz_after_joint_state_broadcaster_spawner = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=joint_state_broadcaster_spawner,
            on_exit=[rviz_node],
        )
    )

    # Delay start of robot_controller after `joint_state_broadcaster`
    delay_robot_drive_controller_spawner_after_joint_state_broadcaster_spawner = (
        RegisterEventHandler(
            event_handler=OnProcessExit(
                target_action=joint_state_broadcaster_spawner,
                on_exit=[robot_drive_controller_spawner],
            )
        )
    )

    delay_dogbot_servo_controller_spawner_after_joint_state_broadcaster_spawner = (
        RegisterEventHandler(
            event_handler=OnProcessExit(
                target_action=joint_state_broadcaster_spawner,
                on_exit=[dogbot_servo_controller_spawner],
            )
        )
    )

    slam_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [
                    FindPackageShare("dogbot_hardware"),
                    "launch",
                    "slam.launch.py",
                ]
            )
        ),
        launch_arguments={
            "use_sim_time": "false",
        }.items(),
    )

    imu_filter_node = Node(
            package="imu_complementary_filter",
            executable="complementary_filter_node",
            output="screen",
            parameters=[
                {
                    "use_mag": True,
                    "do_bias_estimation": True,
                    "do_adaptive_gain": True,
                    "gain_acc": 0.01,
                    "gain_mag": 0.01,
                }
            ],
        )

    imu_publisher_node = Node(
        package="dogbot_imu",
        executable="imu_publisher",
        output="screen",
    )

    nodes = [
        control_node,
        robot_state_pub_node,
        joint_state_broadcaster_spawner,
        delay_rviz_after_joint_state_broadcaster_spawner,
        delay_robot_drive_controller_spawner_after_joint_state_broadcaster_spawner,
        delay_dogbot_servo_controller_spawner_after_joint_state_broadcaster_spawner,
        imu_publisher_node,
        imu_filter_node,
        slam_launch
    ]

    return LaunchDescription(declared_arguments + nodes)
