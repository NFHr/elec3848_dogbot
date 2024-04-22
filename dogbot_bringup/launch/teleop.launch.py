# Copyright 2024 Long Liangmao
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
from launch_ros.actions import Node

def generate_launch_description():
    
    # teleop_twist_keyboard_node
        
        
    teleop_twist_keyboard_node = Node(
        package="teleop_twist_keyboard",
        executable="teleop_twist_keyboard",
        output="screen",
        remappings=[("/cmd_vel", "dogbot_base_controller/cmd_vel")],
        parameters=[
            {
                "speed": 0.1,
                "turn": 0.5,
                "stamped": True,
            }
        ],
        
    )
    

    nodes = [
        teleop_twist_keyboard_node,
    ]

    return LaunchDescription(nodes)