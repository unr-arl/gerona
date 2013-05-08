/*
 * BehaviouralPathDriver.cpp
 *
 *  Created on: Apr 15, 2013
 *      Author: buck <sebastian.buck@uni-tuebingen.de>
 */

/// HEADER
#include "BehaviouralPathDriver.h"

/// PROJECT
#include "MotionControlNode.h"

/// SYSTEM
#include <boost/foreach.hpp>
#include <visualization_msgs/Marker.h>
#include <utils/LibUtil/MathHelper.h>
#include <utils/LibUtil/Line2d.h>
#include <cmath>

using namespace motion_control;

/// BEHAVIOUR BASE
BehaviouralPathDriver::Path& BehaviouralPathDriver::Behaviour::getSubPath(unsigned index)
{
    return parent_.paths_[index];
}
int BehaviouralPathDriver::Behaviour::getSubPathCount() const
{
    return parent_.paths_.size();
}

MotionControlNode& BehaviouralPathDriver::Behaviour::getNode()
{
    return *parent_.node_;
}
double BehaviouralPathDriver::Behaviour::distanceTo(const Waypoint& wp)
{
    return hypot(parent_.slam_pose_(0) - wp.x, parent_.slam_pose_(1) - wp.y);
}
PidCtrl& BehaviouralPathDriver::Behaviour::getPid()
{
    return parent_.pid_;
}
BehaviouralPathDriver::Command& BehaviouralPathDriver::Behaviour::getCommand()
{
    return parent_.current_command_;
}
BehaviouralPathDriver::Options& BehaviouralPathDriver::Behaviour::getOptions()
{
    return parent_.options_;
}

/// SATES / BEHAVIOURS
struct BehaviourEmergencyBreak : public BehaviouralPathDriver::Behaviour {
    BehaviourEmergencyBreak(BehaviouralPathDriver& parent)
        : Behaviour(parent)
    {}

    void execute(int *status)
    {
        *status = MotionResult::MOTION_STATUS_INTERNAL_ERROR;
        throw new BehaviouralPathDriver::NullBehaviour;
    }
};

struct BehaviourDriveBase : public BehaviouralPathDriver::Behaviour {
    BehaviourDriveBase(BehaviouralPathDriver& parent)
        : Behaviour(parent)
    {}

    void getSlamPose() {
        Vector3d slam_pose;
        if ( !getNode().getWorldPose( slam_pose, &slam_pose_msg_ )) {
            *status_ptr_ = MotionResult::MOTION_STATUS_SLAM_FAIL;
            throw new BehaviourEmergencyBreak(parent_);
        }
    }

    double calculateAngleError() {
        return MathHelper::NormalizeAngle(tf::getYaw(next_wp_map_.pose.orientation) - tf::getYaw(slam_pose_msg_.orientation));
    }

    double calculateLineError() {
        BehaviouralPathDriver::Options& opt = getOptions();
        BehaviouralPathDriver::Path& current_path = getSubPath(opt.path_idx);

        geometry_msgs::PoseStamped followup_next_wp_map;
        followup_next_wp_map.pose = current_path[opt.wp_idx + 1];
        followup_next_wp_map.header.stamp = ros::Time::now();

        Line2d target_line;
        Vector3d followup_next_wp_local;
        if (!getNode().transformToLocal( followup_next_wp_map, followup_next_wp_local)) {
            *status_ptr_ = MotionResult::MOTION_STATUS_INTERNAL_ERROR;
            throw new BehaviourEmergencyBreak(parent_);
        }
        target_line = Line2d( next_wp_local_.head<2>(), followup_next_wp_local.head<2>());
        visualizeLine(next_wp_map_, followup_next_wp_map);

        Vector2d carrot, front_pred, rear_pred;
        parent_.predictPose(front_pred, rear_pred);
        if(dir_sign >= 0) {
            carrot = front_pred;
        } else {
            carrot = rear_pred;
        }
        geometry_msgs::PoseStamped carrot_local;
        carrot_local.pose.position.x = carrot[0];
        carrot_local.pose.position.y = carrot[1];

        carrot_local.pose.orientation = tf::createQuaternionMsgFromYaw(0);
        geometry_msgs::PoseStamped carrot_map;
        if (getNode().transformToGlobal(carrot_local, carrot_map)) {
            parent_.drawMark(0, carrot_map.pose.position, "prediction", 0, 0, 0);
        }

        return -target_line.GetSignedDistance(carrot);
    }


    double calculateDistanceError() {
        Vector2d carrot, front_pred, rear_pred;
        parent_.predictPose(front_pred, rear_pred);
        if(dir_sign >= 0) {
            carrot = front_pred;
        } else {
            carrot = rear_pred;
        }
        geometry_msgs::PoseStamped carrot_local;
        carrot_local.pose.position.x = carrot[0];
        carrot_local.pose.position.y = carrot[1];

        carrot_local.pose.orientation = tf::createQuaternionMsgFromYaw(0);
        geometry_msgs::PoseStamped carrot_map;
        if (getNode().transformToGlobal(carrot_local, carrot_map)) {
            parent_.drawMark(0, carrot_map.pose.position, "prediction", 0, 0, 0);
        }

        Vector2d delta = next_wp_local_.head<2>() - carrot;

        if(std::abs(delta(1)) < 0.1) {
            return 0;
        }

        return delta(1);
    }

    void visualizeLine(geometry_msgs::PoseStamped wp_map, geometry_msgs::PoseStamped next_wp) {
        geometry_msgs::Pose target_line_arrow;
        target_line_arrow.position = next_wp.pose.position;
        double dx = next_wp.pose.position.x - wp_map.pose.position.x;
        double dy = next_wp.pose.position.y - wp_map.pose.position.y;

        target_line_arrow.orientation = tf::createQuaternionMsgFromYaw(std::atan2(dy, dx));

        parent_.drawArrow(2, target_line_arrow, "line", 0.7, 0.2, 1.0);
    }

    void setCommand(double error, double speed) {
        double delta_f = 0;
        double delta_r = 0;

        *status_ptr_ = MotionResult::MOTION_STATUS_MOVING;

        if ( !getPid().execute( error, delta_f)) {
            // Nothing to do
            return;
        }

        drawSteeringArrow(0, slam_pose_msg_, delta_f, 1.0, 1.0, 1.0);

        BehaviouralPathDriver::Command& cmd = getCommand();



        double steer = std::max(std::abs(delta_f), std::abs(delta_r));
        ROS_DEBUG_STREAM("dir=" << dir_sign << ", steer=" << steer);
        if(steer > getOptions().steer_slow_threshold_) {
            ROS_WARN_STREAM_THROTTLE(2, "slowing down");
            speed *= 0.5;
        }

        cmd.steer_front = dir_sign * delta_f;
        cmd.steer_back = dir_sign * delta_r;
        cmd.v = dir_sign * speed;
    }

    void drawSteeringArrow(int id, geometry_msgs::Pose steer_arrow, double angle, double r, double g, double b){
        steer_arrow.orientation = tf::createQuaternionMsgFromYaw(tf::getYaw(steer_arrow.orientation) + angle);
        parent_.drawArrow(id, steer_arrow, "steer", r, g, b);
    }

protected:
    int* status_ptr_;

    geometry_msgs::Pose slam_pose_msg_;
    geometry_msgs::PoseStamped next_wp_map_;

    Vector3d next_wp_local_;
    double dir_sign;
};



struct BehaviourOnLine : public BehaviourDriveBase {
    BehaviourOnLine(BehaviouralPathDriver& parent)
        : BehaviourDriveBase(parent)
    {}

    void execute(int *status);
    void getNextWaypoint();
};

struct BehaviourApproachTurningPoint : public BehaviourDriveBase {
    BehaviourApproachTurningPoint(BehaviouralPathDriver& parent)
        : BehaviourDriveBase(parent)
    {}

    void execute(int *status)
    {
        status_ptr_ = status;

        getNextWaypoint();
        getSlamPose();

        dir_sign = MathHelper::sgn(next_wp_local_.x());

        // check if point is reached
        checkIfDone();

        // Calculate target line from current to next waypoint (if there is any)
        double e_distance = calculateDistanceError();
        double e_angle = calculateAngleError();

        double e_combined = e_distance + e_angle;

        // draw steer front
        drawSteeringArrow(1, slam_pose_msg_, e_angle, 0.2, 1.0, 0.2);
        drawSteeringArrow(2, slam_pose_msg_, e_distance, 0.2, 0.2, 1.0);
        drawSteeringArrow(3, slam_pose_msg_, e_combined, 1.0, 0.2, 0.2);

        setCommand(e_combined, 0.1);

        *status_ptr_ = MotionResult::MOTION_STATUS_MOVING;
    }

    void checkIfDone()
    {
        Vector2d delta;
        delta << next_wp_map_.pose.position.x - slam_pose_msg_.position.x,
                next_wp_map_.pose.position.y - slam_pose_msg_.position.y;

        BehaviouralPathDriver::Options& opt = getOptions();
        BehaviouralPathDriver::Path& current_path = getSubPath(opt.path_idx);
        Vector2d target_dir;
        target_dir << std::cos(current_path[opt.wp_idx].theta), std::sin(current_path[opt.wp_idx].theta);

        double angle = MathHelper::AngleClamp(std::atan2(delta(1), delta(0)) - std::atan2(target_dir(1), target_dir(0)));

        ROS_WARN_STREAM("angle=" << angle);

        if(std::abs(angle) >= M_PI / 2) {
            opt.path_idx++;
            opt.wp_idx = 0;

            if(opt.path_idx < getSubPathCount()) {
                *status_ptr_ = MotionResult::MOTION_STATUS_MOVING;
                throw new BehaviourOnLine(parent_);

            } else {
                *status_ptr_ = MotionResult::MOTION_STATUS_SUCCESS;
                throw new BehaviouralPathDriver::NullBehaviour;
            }
        }
    }

    void getNextWaypoint() {
        BehaviouralPathDriver::Options& opt = getOptions();
        BehaviouralPathDriver::Path& current_path = getSubPath(opt.path_idx);

        assert(opt.wp_idx < (int) current_path.size());

        int last_wp_idx = current_path.size() - 2;
        opt.wp_idx = last_wp_idx;

        parent_.drawArrow(0, current_path[opt.wp_idx], "current waypoint", 1, 1, 0);
        parent_.drawArrow(1, current_path[last_wp_idx], "current waypoint", 1, 0, 0);

        next_wp_map_.pose = current_path[opt.wp_idx];
        next_wp_map_.header.stamp = ros::Time::now();

        if ( !getNode().transformToLocal( next_wp_map_, next_wp_local_ )) {
            *status_ptr_ = MotionResult::MOTION_STATUS_SLAM_FAIL;
            throw new BehaviourEmergencyBreak(parent_);
        }
    }
};

void BehaviourOnLine::execute(int *status)
{
    status_ptr_ = status;

    getNextWaypoint();
    getSlamPose();

    dir_sign = MathHelper::sgn(next_wp_local_.x());

    // Calculate target line from current to next waypoint (if there is any)
    double e_distance = calculateLineError();
    double e_angle = calculateAngleError();

    double e_combined = e_distance + e_angle;

    // draw steer front
    drawSteeringArrow(1, slam_pose_msg_, e_angle, 0.2, 1.0, 0.2);
    drawSteeringArrow(2, slam_pose_msg_, e_distance, 0.2, 0.2, 1.0);
    drawSteeringArrow(3, slam_pose_msg_, e_combined, 1.0, 0.2, 0.2);

    double speed = getOptions().max_speed_;

    if(dir_sign < 0) {
        speed *= 0.5;
    }

    setCommand(e_combined, speed);

    *status_ptr_ = MotionResult::MOTION_STATUS_MOVING;
}

void BehaviourOnLine::getNextWaypoint() {
    BehaviouralPathDriver::Options& opt = getOptions();
    BehaviouralPathDriver::Path& current_path = getSubPath(opt.path_idx);

    assert(opt.wp_idx < (int) current_path.size());

    int last_wp_idx = current_path.size() - 1;

    // if distance to wp < threshold
    while(distanceTo(current_path[opt.wp_idx]) < opt.wp_tolerance_) {
        if(opt.wp_idx >= last_wp_idx) {
            // if distance to wp == last_wp -> state = APPROACH_TURNING_POINT
            *status_ptr_ = MotionResult::MOTION_STATUS_MOVING;
            throw new BehaviourApproachTurningPoint(parent_);

        } else {
            // else choose next wp
            opt.wp_idx++;
        }
    }

    parent_.drawArrow(0, current_path[opt.wp_idx], "current waypoint", 1, 1, 0);
    parent_.drawArrow(1, current_path[last_wp_idx], "current waypoint", 1, 0, 0);

    next_wp_map_.pose = current_path[opt.wp_idx];
    next_wp_map_.header.stamp = ros::Time::now();

    if ( !getNode().transformToLocal( next_wp_map_, next_wp_local_ )) {
        *status_ptr_ = MotionResult::MOTION_STATUS_SLAM_FAIL;
        throw new BehaviourEmergencyBreak(parent_);
    }
}





BehaviouralPathDriver::BehaviouralPathDriver(ros::Publisher &cmd_pub, MotionControlNode *node)
    : node_(node), private_nh_("~"), cmd_pub_(cmd_pub), active_behaviour_(NULL), pending_error_(-1)
{
    vis_pub_ = private_nh_.advertise<visualization_msgs::Marker>("/marker", 100);
    configure();
}

void BehaviouralPathDriver::start()
{
    options_.reset();

    clearActive();

    active_behaviour_ = new BehaviourOnLine(*this);
    ROS_INFO_STREAM("init with " << typeid(*active_behaviour_).name());
}

void BehaviouralPathDriver::stop()
{
    clearActive();

    current_command_.v = 0;
}

int BehaviouralPathDriver::getType()
{
    return motion_control::MotionGoal::MOTION_FOLLOW_PATH;
}


int BehaviouralPathDriver::execute(MotionFeedback& fb, MotionResult& result)
{
    // Pending error?
    if ( pending_error_ >= 0 ) {
        int error = pending_error_;
        pending_error_ = -1;
        stop();
        return error;
    }

    if(paths_.empty()) {
        clearActive();
        return MotionResult::MOTION_STATUS_SUCCESS;
    }

    if(active_behaviour_ == NULL) {
        start();
    }

    geometry_msgs::Pose slampose_p;
    if ( !node_->getWorldPose( slam_pose_, &slampose_p )) {
        stop();
        return MotionResult::MOTION_STATUS_SLAM_FAIL;
    }

    drawArrow(0, slampose_p, "slam pose", 2.0, 0.7, 1.0);


    int status = MotionResult::MOTION_STATUS_INTERNAL_ERROR;
    try {
        //        ROS_INFO_STREAM("executing " << typeid(*active_behaviour_).name());
        active_behaviour_->execute(&status);

    } catch(NullBehaviour* null) {
        ROS_WARN_STREAM("stopping after " << typeid(*active_behaviour_).name());
        clearActive();

        assert(status == MotionResult::MOTION_STATUS_SUCCESS);

        current_command_.v = 0;

    } catch(Behaviour* next_behaviour) {
        ROS_WARN_STREAM("switching behaviour from " << typeid(*active_behaviour_).name() << " to " << typeid(*next_behaviour).name() );
        clearActive();
        active_behaviour_ = next_behaviour;
    }

    publishCommand();

    if(status != MotionResult::MOTION_STATUS_MOVING && active_behaviour_ != NULL) {
        ROS_INFO_STREAM("aborting, clearing active, status=" << status);
        clearActive();
    }

    return status;
}

void BehaviouralPathDriver::configure()
{
    ros::NodeHandle nh("~");
    nh.param( "dead_time", options_.dead_time_, 0.10 );
    nh.param( "waypoint_tolerance", options_.wp_tolerance_, 0.20 );
    nh.param( "goal_tolerance", options_.goal_tolerance_, 0.15 );
    nh.param( "l", options_.l_, 0.38 );
    nh.param( "steer_slow_threshold", options_.steer_slow_threshold_, 0.25);

    double ta, kp, ki, i_max, delta_max, e_max;
    nh.param( "pid/ta", ta, 0.03 );
    nh.param( "pid/kp", kp, 1.5 );
    nh.param( "pid/ki", ki, 0.001 );
    nh.param( "pid/i_max", i_max, 0.0 );
    nh.param( "pid/delta_max", delta_max, 30.0 );
    nh.param( "pid/e_max", e_max, 0.10 );

    pid_.configure( kp, ki, i_max, M_PI*delta_max/180.0, e_max, 0.5, ta );
}

void BehaviouralPathDriver::setPath(const nav_msgs::Path& path)
{
    path_ = path;

    paths_.clear();

    // find segments
    unsigned n = path_.poses.size();
    if(n < 2) {
        return;
    }

    Path current_segment;

    Waypoint last_point(path_.poses[0]);
    current_segment.push_back(last_point);

    int id = 0;

    for(unsigned i = 1; i < n; ++i){
        const Waypoint current_point(path_.poses[i]);

        // append to current segment
        current_segment.push_back(current_point);

        bool is_the_last_node = i == n-1;
        bool segment_ends_with_this_node = false;

        if(is_the_last_node) {
            // this is the last node
            segment_ends_with_this_node = true;

        } else {
            const Waypoint next_point(path_.poses[i+1]);

            // if angle between last direction and next direction to large -> segment ends
            double diff_last_x = current_point.x - last_point.x;
            double diff_last_y = current_point.y - last_point.y;
            double last_angle = std::atan2(diff_last_y, diff_last_x);

            double diff_next_x = next_point.x - current_point.x;
            double diff_next_y = next_point.y - current_point.y;
            double next_angle = std::atan2(diff_next_y, diff_next_x);

            double angle = MathHelper::AngleClamp(last_angle - next_angle);

            if(std::abs(angle) > M_PI / 3.0) {
                // new segment!
                // current node is the last one of the old segment
                segment_ends_with_this_node = true;
            }
        }

        ROS_INFO_STREAM("drawing #" << id);
        drawArrow(id++, current_point, "paths", 0, 0, 0);
        if(segment_ends_with_this_node) {
            //            drawArrow(id++, current_point, "paths", 0, 0, 0);


            paths_.push_back(current_segment);
            current_segment.clear();

            if(!is_the_last_node) {
                // begin new segment
                // current node is also the first one of the new segment
                current_segment.push_back(current_point);
            }
        }

        last_point = current_point;
    }
}
void BehaviouralPathDriver::predictPose(Vector2d &front_pred, Vector2d &rear_pred)
{
    double dt = options_.dead_time_;
    double deltaf = current_command_.steer_front;
    double deltar = current_command_.steer_back;
    double v = 2*getFilteredSpeed();

    double beta = std::atan(0.5*(std::tan(deltaf)+std::tan(deltar)));
    double ds = v*dt;
    double dtheta = ds*std::cos(beta)*(std::tan(deltaf)-std::tan(deltar))/options_.l_;
    double thetan = dtheta;
    double yn = ds*std::sin(dtheta*0.5+beta*0.5);
    double xn = ds*std::cos(dtheta*0.5+beta*0.5);

    front_pred[0] = xn+cos(thetan)*options_.l_/2.0;
    front_pred[1] = yn+sin(thetan)*options_.l_/2.0;
    rear_pred[0] = xn-cos(thetan)*options_.l_/2.0;
    rear_pred[1] = yn-sin(thetan)*options_.l_/2.0;
}

void BehaviouralPathDriver::drawArrow(int id, const geometry_msgs::Pose& pose, const std::string& ns, float r, float g, float b, double live)
{
    visualization_msgs::Marker marker;
    marker.pose = pose;
    marker.ns = ns;
    marker.header.frame_id = "/map";
    marker.header.stamp = ros::Time();
    marker.action = visualization_msgs::Marker::ADD;
    marker.id = id;
    marker.lifetime = ros::Duration(live);
    marker.color.r = r;
    marker.color.g = g;
    marker.color.b = b;
    marker.color.a = 1.0;
    marker.scale.x = 0.75;
    marker.scale.y = 0.05;
    marker.scale.z = 0.05;
    marker.type = visualization_msgs::Marker::ARROW;
    vis_pub_.publish(marker);
}

void BehaviouralPathDriver::drawMark(int id, const geometry_msgs::Point &pos, const string &ns, float r, float g, float b)
{
    visualization_msgs::Marker marker;
    marker.pose.position = pos;
    marker.ns = ns;
    marker.header.frame_id = "/map";
    marker.header.stamp = ros::Time();
    marker.action = visualization_msgs::Marker::ADD;
    marker.id = id;
    marker.lifetime = ros::Duration(3);
    marker.color.r = r;
    marker.color.g = g;
    marker.color.b = b;
    marker.color.a = 1.0;
    marker.scale.x = 0.1;
    marker.scale.y = 0.1;
    marker.scale.z = 0.5;
    marker.type = visualization_msgs::Marker::CUBE;
    vis_pub_.publish(marker);
}


void BehaviouralPathDriver::setGoal(const motion_control::MotionGoal& goal)
{
    pending_error_ = -1;
    options_.max_speed_ = goal.v;

    if ( goal.path.poses.size() < 2 ) {
        ROS_ERROR( "Got an invalid path with less than two poses." );
        stop();
        pending_error_ = MotionResult::MOTION_STATUS_INTERNAL_ERROR;
        return;
    }

    setPath(goal.path);

    ROS_INFO_STREAM("Following path with " << goal.path.poses.size() << " poses.");
}

void BehaviouralPathDriver::clearActive()
{
    if(active_behaviour_ != NULL) {
        delete active_behaviour_;
    }
    active_behaviour_ = NULL;
}

void BehaviouralPathDriver::publishCommand()
{
    ramaxx_msgs::RamaxxMsg msg = current_command_;
    cmd_pub_.publish(msg);

    setFilteredSpeed(current_command_.v);
}
