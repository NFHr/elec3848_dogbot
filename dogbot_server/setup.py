from setuptools import find_packages, setup

package_name = 'dogbot_server'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='fn',
    maintainer_email='87050903+NFHr@users.noreply.github.com',
    description='TODO: Package description',
    license='Apache-2.0',
    entry_points={
        'console_scripts': [
            'dogbot_server = dogbot_server.dogbot_server:main',
        ],
    },
)
