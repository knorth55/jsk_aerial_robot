// -*- mode: c++ -*-
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2021, JSK Lab
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/o2r other materials provided
 *     with the distribution.
 *   * Neither the name of the JSK Lab nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#include <dragon/sensor/imu.h>


namespace sensor_plugin
{
  void DragonImu::initialize(ros::NodeHandle nh,
                  boost::shared_ptr<aerial_robot_model::RobotModel> robot_model,
                  boost::shared_ptr<aerial_robot_estimation::StateEstimator> estimator,
                  string sensor_name, int index)
  {
    Imu::initialize(nh, robot_model, estimator, std::string("sensor_plugin/imu"), index);
  }


  // override to get filtred gyro data
  void DragonImu::ImuCallback(const spinal::ImuConstPtr& imu_msg)
  {
    imu_stamp_ = imu_msg->stamp;
    tf::Vector3 filtered_omega;

    for(int i = 0; i < 3; i++)
      {
        if(std::isnan(imu_msg->acc_data[i]) || std::isnan(imu_msg->angles[i]) ||
           std::isnan(imu_msg->gyro_data[i]) || std::isnan(imu_msg->mag_data[i]))
          {
            ROS_ERROR_THROTTLE(1.0, "IMU sensor publishes Nan value!");
            return;
          }

        acc_b_[i] = imu_msg->acc_data[i];
        euler_[i] = imu_msg->angles[i];
        omega_[i] = imu_msg->gyro_data[i];
        filtered_omega[i] = imu_msg->gyro_data[i];
      }

    // workaround: use raw roll&pitch omega (not filtered in spinal) for both angular and linear CoG velocity estimation, yaw is still filtered
    // note: this is different with hydrus-like control which use filtered omega for CoG estimation
    omega_[0] = imu_msg->mag_data[0];
    omega_[1] = imu_msg->mag_data[1];

    // TODO:
    // 1. use heavily filtered omege to calculate cog velocity, although this is not good way to get baselink -> cog velocity, position
    // 2. use lightly filterd omega to calculate cog angular velocity

    // workaround to get filtered angular and linear velocity of CoG
    tf::Transform cog2baselink_tf;
    tf::transformKDLToTF(robot_model_->getCog2Baselink<KDL::Frame>(), cog2baselink_tf);
    int estimate_mode = estimator_->getEstimateMode();
    setFilteredOmegaCog(cog2baselink_tf.getBasis() * filtered_omega);
    setFilteredVelCog(estimator_->getVel(Frame::BASELINK, estimate_mode)
                      + estimator_->getOrientation(Frame::BASELINK, estimate_mode)
                      * (filtered_omega.cross(cog2baselink_tf.inverse().getOrigin())));

    estimateProcess();
    updateHealthStamp();
  }

};
/* plugin registration */
#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(sensor_plugin::DragonImu, sensor_plugin::SensorBase);



