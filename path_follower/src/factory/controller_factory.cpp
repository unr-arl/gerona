/// HEADER
#include <path_follower/factory/controller_factory.h>

#include <path_follower/pathfollower.h>

// Controller/Models
#include <path_follower/controller/robotcontroller_ackermann_pid.h>
#include <path_follower/controller/robotcontrollertrailer.h>
#include <path_follower/controller/robotcontroller_ICR_CCW.h>
#include <path_follower/controller/robotcontroller_ackermann_orthexp.h>
#include <path_follower/controller/robotcontroller_ackermann_purepursuit.h>
#include <path_follower/controller/robotcontroller_ackermann_inputscaling.h>
#include <path_follower/controller/robotcontroller_ackermann_stanley.h>
#include <path_follower/controller/robotcontroller_2steer_purepursuit.h>
#include <path_follower/controller/robotcontroller_2steer_stanley.h>
#include <path_follower/controller/robotcontroller_2steer_inputscaling.h>
#include <path_follower/controller/robotcontroller_unicycle_inputscaling.h>
#include <path_follower/controller/robotcontroller_omnidrive_orthexp.h>
#include <path_follower/controller/robotcontroller_differential_orthexp.h>
#include <path_follower/controller/robotcontroller_kinematic_SLP.h>
#include <path_follower/controller/robotcontroller_dynamic_SLP.h>
#include <path_follower/controller/robotcontroller_kinematic_HBZ.h>

#include <path_follower/obstacle_avoidance/noneavoider.hpp>
#include <path_follower/obstacle_avoidance/obstacledetectorackermann.h>
#include <path_follower/obstacle_avoidance/obstacledetectoromnidrive.h>
#include <path_follower/obstacle_avoidance/obstacledetectorpatsy.h>

#include <path_follower/local_planner/local_planner_null.h>
#include <path_follower/local_planner/local_planner_transformer.h>
#include <path_follower/local_planner/local_planner_bfs_static.h>
#include <path_follower/local_planner/local_planner_bfs_reconf.h>
#include <path_follower/local_planner/local_planner_astar_n_static.h>
#include <path_follower/local_planner/local_planner_astar_g_static.h>
#include <path_follower/local_planner/local_planner_astar_n_reconf.h>
#include <path_follower/local_planner/local_planner_astar_g_reconf.h>
#include <path_follower/local_planner/local_planner_thetastar_n_static.h>
#include <path_follower/local_planner/local_planner_thetastar_g_static.h>
#include <path_follower/local_planner/local_planner_thetastar_n_reconf.h>
#include <path_follower/local_planner/local_planner_thetastar_g_reconf.h>

#include <path_follower/utils/pose_tracker.h>

// SYSTEM
#include <stdexcept>

ControllerFactory::ControllerFactory(PathFollower &follower)
    : follower_(follower), opt_(follower.getOptions()), pose_tracker_(follower.getPoseTracker())
{

}

void ControllerFactory::construct(std::shared_ptr<RobotController>& out_controller,
               std::shared_ptr<LocalPlanner>& out_local_planner,
               std::shared_ptr<ObstacleAvoider>& out_obstacle_avoider)
{
    const std::string& name = opt_.controller();

    ROS_INFO("Use robot controller '%s'", name.c_str());
    if (name == "ackermann_pid") {
        out_controller = std::make_shared<RobotController_Ackermann_Pid>();

    } else if (name == "ackermann_purepursuit") {
        out_controller = std::make_shared<Robotcontroller_Ackermann_PurePursuit>();

    } else if (name == "ackermann_inputscaling") {
        out_controller = std::make_shared<RobotController_Ackermann_Inputscaling>();

    } else if (name == "ackermann_stanley") {
        out_controller = std::make_shared<RobotController_Ackermann_Stanley>();

    } else if (name == "2steer_purepursuit") {
        out_controller = std::make_shared<RobotController_2Steer_PurePursuit>();

    } else if (name == "2steer_stanley") {
        out_controller = std::make_shared<RobotController_2Steer_Stanley>();

    } else if (name == "2steer_inputscaling") {
        out_controller = std::make_shared<RobotController_2Steer_InputScaling>();

    } else if (name == "unicycle_inputscaling") {
        out_controller = std::make_shared<RobotController_Unicycle_InputScaling>();

    } else if (name == "patsy_pid") {
        out_controller = std::make_shared<RobotControllerTrailer>();

    } else if (name == "omnidrive_orthexp") {
        out_controller = std::make_shared<RobotController_Omnidrive_OrthogonalExponential>();

    } else if (name == "ackermann_orthexp") {
        out_controller = std::make_shared<RobotController_Ackermann_OrthogonalExponential>();

    } else if (name == "differential_orthexp") {
        out_controller = std::make_shared<RobotController_Differential_OrthogonalExponential>();

    } else if (name == "kinematic_SLP") {
        out_controller = std::make_shared<RobotController_Kinematic_SLP>();

    } else if (name == "dynamic_SLP") {
        out_controller = std::make_shared<RobotController_Dynamic_SLP>();

    } else if (name == "kinematic_HBZ") {
        out_controller = std::make_shared<RobotController_Kinematic_HBZ>();

    } else if (name == "ICR_CCW") {
        out_controller = std::make_shared<RobotController_ICR_CCW>();

    } else {
        throw std::logic_error("Unknown robot controller. Shutdown.");
    }


    ROS_INFO("Use local planner algorithm '%s'", name.c_str());

    ROS_INFO("Maximum number of allowed nodes: %d", opt_.nnodes());
    ROS_INFO("Maximum tree depth: %d", opt_.depth());
    ROS_INFO("Update Interval: %.3f", opt_.uinterval());
    ROS_INFO("Maximal distance from path: %.3f", opt_.dis2p());
    ROS_INFO("Security distance around the robot: %.3f", opt_.adis());
    ROS_INFO("Security distance in front of the robot: %.3f", opt_.fdis());
    ROS_INFO("Steering angle: %.3f", opt_.s_angle());
    ROS_INFO("Intermediate Configurations: %d",opt_.ic());
    ROS_INFO("Intermediate Angles: %d",opt_.ia());
    ROS_INFO("Using current velocity: %s",opt_.use_v() ? "true" : "false");
    ROS_INFO("Length multiplying factor: %.3f",opt_.lmf());
    ROS_INFO("Coefficient of friction: %.3f",opt_.mu());
    ROS_INFO("Exponent factor: %.3f",opt_.ef());

    ROS_INFO("Constraint usage [%s, %s]", opt_.c1() ? "true" : "false",
             opt_.c2() ? "true" : "false");
    ROS_INFO("Scorer usage [%.3f, %.3f, %.3f, %.3f, %.3f, %.3f]", opt_.s1(),opt_.s2(),
             opt_.s3(), opt_.s4(), opt_.s5(), opt_.s6());


    const std::string& local_planner_name = opt_.algo();

    if(local_planner_name == "AStar"){
        out_local_planner = std::make_shared<LocalPlannerAStarNStatic>();
    }else if(local_planner_name == "AStarG"){
        out_local_planner = std::make_shared<LocalPlannerAStarGStatic>();
    }else if(local_planner_name == "ThetaStar"){
        out_local_planner = std::make_shared<LocalPlannerThetaStarNStatic>();
    }else if(local_planner_name == "ThetaStarG"){
        out_local_planner = std::make_shared<LocalPlannerThetaStarGStatic>();
    }else if(local_planner_name == "AStarR"){
        out_local_planner = std::make_shared<LocalPlannerAStarNReconf>();
    }else if(local_planner_name == "AStarGR"){
        out_local_planner = std::make_shared<LocalPlannerAStarGReconf>();
    }else if(local_planner_name == "ThetaStarR"){
        out_local_planner = std::make_shared<LocalPlannerThetaStarNReconf>();
    }else if(local_planner_name == "ThetaStarGR"){
        out_local_planner = std::make_shared<LocalPlannerThetaStarGReconf>();
    }else if(local_planner_name == "BFS"){
        out_local_planner = std::make_shared<LocalPlannerBFSStatic>();
    }else if(local_planner_name == "BFSR"){
        out_local_planner = std::make_shared<LocalPlannerBFSReconf>();
    }else if(local_planner_name == "Transformer"){
        out_local_planner = std::make_shared<LocalPlannerTransformer>();
    }else if(local_planner_name == "NULL"){
        out_local_planner = std::make_shared<LocalPlannerNull>();
    }else {
        throw std::logic_error("Unknown local planner algorithm. Shutdown.");
    }


    ROS_INFO("Use robot controller '%s'", name.c_str());
    if (name == "ackermann_pid") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "ackermann_purepursuit") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "ackermann_inputscaling") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "ackermann_stanley") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "2steer_purepursuit") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "2steer_stanley") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "2steer_inputscaling") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "unicycle_inputscaling") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "patsy_pid") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorPatsy>();

    } else if (name == "omnidrive_orthexp") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorOmnidrive>();

    } else if (name == "ackermann_orthexp") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorOmnidrive>();

    } else if (name == "differential_orthexp") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorOmnidrive>();

    } else if (name == "kinematic_SLP") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "dynamic_SLP") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "kinematic_HBZ") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else if (name == "ICR_CCW") {
        if (opt_.obstacle_avoider_use_collision_box())
            out_obstacle_avoider = std::make_shared<ObstacleDetectorAckermann>();

    } else {
        throw std::logic_error("Unknown robot controller. Shutdown.");
    }

    //  if no obstacle avoider was set, use the none-avoider
    out_obstacle_avoider = std::make_shared<NoneAvoider>();


    // wiring
    out_obstacle_avoider->setTransformListener(&pose_tracker_.getTransformListener());

    ros::Duration uinterval(opt_.uinterval());
    out_local_planner->init(out_controller.get(), &pose_tracker_, uinterval);

    out_controller->init(&pose_tracker_, out_obstacle_avoider.get(), &opt_);

    pose_tracker_.setLocal(!out_local_planner->isNull());

    out_local_planner->setParams(opt_.nnodes(), opt_.ic(), opt_.dis2p(), opt_.adis(),
                       opt_.fdis(),opt_.s_angle(), opt_.ia(), opt_.lmf(),
                       opt_.depth(), opt_.mu(), opt_.ef());
}
