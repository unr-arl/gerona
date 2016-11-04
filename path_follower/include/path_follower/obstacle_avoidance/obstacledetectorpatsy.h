#ifndef OBSTACLEDETECTORPATSY_H
#define OBSTACLEDETECTORPATSY_H
#include <opencv2/core/core.hpp>
#include <tf/transform_listener.h>
#include <path_follower/obstacle_avoidance/obstacledetector.h>
class RobotControllerTrailer ;
/**
 * @brief Rectangle obstacle box that is enlarged and bend in curves
 *
 * See comments in the code (*.cpp) for details of the behaviour of the
 * box in curves.
 */
class ObstacleDetectorPatsy : public ObstacleDetector
{
public:
    ObstacleDetectorPatsy();

    virtual bool avoid(MoveCommand * const cmd,
                       const ObstacleAvoider::State &state);

    virtual bool checkOnCloud(std::shared_ptr<ObstacleCloud const> obstacles,
                              float width,
                              float length,
                              float course_angle,
                              float curve_enlarge_factor);

protected:
    virtual void getPolygon(float width, float length,  float course_angle, float curve_enlarge_factor, std::vector<cv::Point2f>& polygon) const;
    void visualize(std::vector<cv::Point2f>& polygon,const std::string& frame, bool hasObstacle) const;

    const tf::TransformListener *tf_listener_;

    std::string front_frame_, rear_frame_;


};
#endif // OBSTACLEDETECTORPATSY_H
