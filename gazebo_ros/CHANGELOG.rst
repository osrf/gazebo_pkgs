^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package gazebo_ros
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

2.1.3 (2013-07-13)
------------------

2.1.2 (2013-07-12)
------------------
* Added author
* Tweak to make SDFConfig.cmake
* Cleaned up CMakeLists.txt for all gazebo_ros_pkgs
* Cleaned up gazebo_ros_paths_plugin
* 2.1.1

2.1.1 (2013-07-10 19:11)
------------------------
* Merge branch 'hydro-devel' of github.com:ros-simulation/gazebo_ros_pkgs into hydro-devel
* Reduced number of debug msgs
* Fixed physics dynamic reconfigure namespace
* gazebo_ros_api_plugin: set plugin_loaded_ flag to true in
  GazeboRosApiPlugin::Load() function
* Actually we need __init__.py
* Cleaning up code
* Moved gazebo_interface.py from gazebo/ folder to gazebo_ros/ folder
* Removed searching for plugins under 'gazebo' pkg because of rospack warnings
* Minor print modification
* Added dependency to prevent missing msg header, cleaned up CMakeLists

2.1.0 (2013-06-27)
------------------
* gazebo_ros: added deprecated warning for packages that use gazebo as
  package name for exported paths
* Hiding some debug info
* gazebo_ros: use rosrun in debug script, as rospack find gazebo_ros returns the wrong path in install space
* Hide Model XML debut output to console
* gazebo_ros_api_plugin.h is no longer exposed in the include folder
* Added args to launch files, documentation
* Merge pull request `#28 <https://github.com/ros-simulation/gazebo_ros_pkgs/issues/28>`_ from osrf/no_roscore_handling
  Better handling of gazebo_ros run when no roscore started
* gazebo_ros: also support gazebo instead of gazebo_ros as package name for plugin_path, gazebo_model_path or gazebo_media_path exports
* gazebo_plugins/gazebo_ros: fixed install directories for include files and gazebo scripts
* changed comment location
* added block comments for walkChildAddRobotNamespace
* SDF and URDF now set robotNamespace for plugins
* Better handling of gazebo_ros run when no roscore started

2.0.2 (2013-06-20)
------------------
* Added Gazebo dependency
* changed the final kill to send a SIGINT and ensure only the last background process is killed.
* modified script to work in bash correctly (tested on ubuntu 12.04 LTS)

2.0.1 (2013-06-19)
------------------
* Incremented version to 2.0.1
* Fixed circular dependency, removed deprecated pkgs since its a stand alone pkg
* Shortened line lengths of function headers

2.0.0 (2013-06-18)
------------------
* Changed version to 2.0.0 based on gazebo_simulator being 1.0.0
* Updated package.xml files for ros.org documentation purposes
* Combined updateSDFModelPose and updateSDFName, added ability to spawn SDFs from model database, updates SDF version to lastest in parts of code, updated the tests
* Renamed Gazebo model to SDF model, added ability to spawn from online database
* Fixed really obvious error checking bug
* Deprecated -gazebo arg in favor of -sdf tag
* Reordered services and messages to be organized and reflect documentation. No code change
* Cleaned up file, addded debug info
* Merged changes from Atlas ROS plugins, cleaned up headers
* Small fixes per ffurrer's code review
* Deprecated warnings fixes
* Cleaned up comment blocks - removed from .cpp and added to .h
* Merged branches and more small cleanups
* Small compile error fix
* Standardized function and variable naming convention, cleaned up function comments
* Reduced debug output and refresh frequency of robot spawner
* Converted all non-Gazebo pointers to boost shared_ptrs
* Removed old Gazebo XML handling functions - has been replaced by SDF, various code cleanup
* Removed the physics reconfigure node handle, switched to async ROS spinner, reduced required while loops
* Fixed shutdown segfault, renamed rosnode_ to nh_, made all member variables have _ at end, formatted functions
* Added small comment
* adding install for gazebo_ros launchfiles
* Formatted files to be double space indent per ROS standards
* Started fixing thread issues
* Fixing install script names and adding gzserver and gdbrun to install command
* Fixed deprecated warnings, auto formatted file
* Cleaned up status messages
* Added -h -help --help arguemnts to spawn_model
* Removed broken worlds
* Removed deprecated namespace argument
* Using pkg-config to find the script installation path.
  Corrected a bash typo with client_final variable in gazebo script.
* Cleaning up world files
* Deprecated fix
* Moved from gazebo_worlds
* Cleaning up launch files
* Moved from gazebo_worlds
* Fixing renaming errors
* Updated launch and world files and moved to gazebo_ros
* Combined gzclient and gzserver
* Added finished loading msg
* All packages building in Groovy/Catkin
* Imported from bitbucket.org
