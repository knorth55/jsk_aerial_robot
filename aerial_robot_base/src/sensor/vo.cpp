// -*- mode: c++ -*-
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2017, JSK Lab
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

/* ros */
#include <ros/ros.h>

/* base class */
#include <aerial_robot_base/sensor_base_plugin.h>

/* kalman filters */
#include <kalman_filter/kf_pos_vel_acc_plugin.h>

/* ros msg */
#include <geometry_msgs/Vector3Stamped.h>
#include <nav_msgs/Odometry.h>
#include <tf/transform_listener.h>

namespace sensor_plugin
{
  class VisualOdometry :public sensor_plugin::SensorBase
  {
  public:

    void initialize(ros::NodeHandle nh, ros::NodeHandle nhp, BasicEstimator* estimator, string sensor_name)
    {
      SensorBase::initialize(nh, nhp, estimator, sensor_name);
      rosParamInit();

      /* ros publisher: aerial_robot_base::State */
      vo_state_pub_ = nh_.advertise<aerial_robot_msgs::States>("data",10);

      /* ros subscriber: vo */
      string vo_sub_topic_name;
      nhp_.param("vo_sub_topic_name", vo_sub_topic_name, string("vo") );
      vo_sub_ = nh_.subscribe(vo_sub_topic_name, 1, &VisualOdometry::voCallback, this);

    }

    ~VisualOdometry(){}
    VisualOdometry():init_time_(true)
    {
      world_offset_tf_.setIdentity();
      baselink_tf_.setIdentity();

      vo_state_.states.resize(3);
      vo_state_.states[0].id = "x";
      vo_state_.states[0].state.resize(2);
      vo_state_.states[1].id = "y";
      vo_state_.states[1].state.resize(2);
      vo_state_.states[2].id = "z";
      vo_state_.states[2].state.resize(2);
    }

  private:
    /* ros */
    ros::Subscriber vo_sub_;
    ros::Publisher vo_state_pub_;

    /* ros param */
    double vo_noise_sigma_;
    bool valid_yaw_;
    bool vio_flag_;
    bool debug_verbose_;

    bool init_time_;
    tf::StampedTransform sensor_tf_; /* TODO: this should be in the basic estimation */
    tf::Transform world_offset_tf_; /* TODO: this should be in the basic estimation */
    tf::Transform baselink_tf_;
    aerial_robot_msgs::States vo_state_;

    void voCallback(const nav_msgs::Odometry::ConstPtr & vo_msg)
    {
      /* only do egmotion estimate mode */
      if(!getFuserActivate(BasicEstimator::EGOMOTION_ESTIMATE))
        {
          ROS_WARN_THROTTLE(1,"Visual Odometry: no egmotion estimate mode");
          return;
        }

      tf::Transform raw_sensor_tf;
      tf::poseMsgToTF(vo_msg->pose.pose, raw_sensor_tf);

      if(init_time_)
        {
          init_time_ = false;

          /* get transform from baselink to vo frame */
          /* TODO1: this should be in the basic estimation */
          /* TODO2: for joint or servo system, this should be processed every time */
          {
            string sensor_frame, reference_frame;
            nhp_.param("sensor_frame", sensor_frame, string("sensor_frame"));
            nhp_.param("reference_frame", reference_frame, string("fc"));

            tf::TransformListener listener;
            try
              {
                listener.waitForTransform(reference_frame, sensor_frame, ros::Time(0), ros::Duration(1.0));
                listener.lookupTransform(reference_frame, sensor_frame, ros::Time(0), sensor_tf_);
              }
            catch (tf::TransformException ex)
              {
                ROS_ERROR("%s: %s",nhp_.getNamespace().c_str(), ex.what());
                init_time_ = true;
                return;
              }
            double y, p, r; sensor_tf_.getBasis().getRPY(r, p, y);
            ROS_INFO("%s: get tf from %s to %s, [%f, %f, %f], [%f, %f, %f]",
                     nhp_.getNamespace().c_str(), reference_frame.c_str(), sensor_frame.c_str(),
                     sensor_tf_.getOrigin().x(), sensor_tf_.getOrigin().y(),
                     sensor_tf_.getOrigin().z(), r, p, y);
          }

          /* NO necessary: only consider the yaw angle between baselink and vo sensor, since the vo odom frame is flat
          if(vio_flag_)
            {
              double sensor_tf_yaw = getYaw(sensor_tf_.getRotation());
              sensor_tf_.setRotation(tf::createQuaternionFromYaw(sensor_tf_yaw));
            }
          */

          /* set the init offset from world to the baselink of UAV from egomotion estimation (e.g. yaw) */
          world_offset_tf_.setRotation(tf::createQuaternionFromYaw(estimator_->getState(State::YAW_BASE, BasicEstimator::EGOMOTION_ESTIMATE)[0]));
          /* set the init offset from world to the baselink of UAV if we know the ground truth */
          if(estimator_->getStateStatus(State::YAW_BASE, BasicEstimator::GROUND_TRUTH))
            {
              world_offset_tf_.setOrigin(estimator_->getPos(Frame::BASELINK, BasicEstimator::GROUND_TRUTH));
              world_offset_tf_.setRotation(tf::createQuaternionFromYaw(estimator_->getState(State::YAW_BASE, BasicEstimator::GROUND_TRUTH)[0]));

              double y, p, r; world_offset_tf_.getBasis().getRPY(r, p, y);
            }

          /* also consider the offset tf from baselink to sensor */
          world_offset_tf_ *= sensor_tf_;

          if(!estimator_->getStateStatus(State::X_BASE, BasicEstimator::EGOMOTION_ESTIMATE) ||
             !estimator_->getStateStatus(State::Y_BASE, BasicEstimator::EGOMOTION_ESTIMATE))
            {
              ROS_INFO("VO: start/restart kalman filter");
              tf::Vector3 init_pos = (world_offset_tf_ * raw_sensor_tf * sensor_tf_.inverse()).getOrigin();

              for(auto& fuser : estimator_->getFuser(BasicEstimator::EGOMOTION_ESTIMATE))
                {
                  boost::shared_ptr<kf_plugin::KalmanFilter> kf = fuser.second;
                  int id = kf->getId();

                  if(id < (1 << State::ROLL_COG))
                    {
                      if(time_sync_) kf->setTimeSync(true);
                      kf->setInitState(init_pos[id >> (State::X_BASE + 1)], 0);
                      kf->setMeasureFlag();
                    }
                }
            }
          estimator_->setStateStatus(State::X_BASE, BasicEstimator::EGOMOTION_ESTIMATE, true);
          estimator_->setStateStatus(State::Y_BASE, BasicEstimator::EGOMOTION_ESTIMATE, true);
          estimator_->setStateStatus(State::Z_BASE, BasicEstimator::EGOMOTION_ESTIMATE, true);
          estimator_->setStateStatus(State::YAW_BASE, BasicEstimator::EGOMOTION_ESTIMATE, true);
          return;
        }

      baselink_tf_ = world_offset_tf_ * raw_sensor_tf * sensor_tf_.inverse();


      tf::Vector3 raw_pos;
      tf::pointMsgToTF(vo_msg->pose.pose.position, raw_pos);
      tf::Quaternion raw_q;
      tf::quaternionMsgToTF(vo_msg->pose.pose.orientation, raw_q);

      if(debug_verbose_)
        {
          double y, p, r;  tf::Matrix3x3(raw_q).getRPY(r, p, y);
          ROS_INFO("vo raw pos: [%f, %f, %f], raw rot: [%f, %f, %f]",
                   raw_pos.x(), raw_pos.y(), raw_pos.z(), r, p, y);
          tf::Vector3 mocap_pos = estimator_->getPos(Frame::BASELINK, BasicEstimator::GROUND_TRUTH);
          ROS_INFO("mocap pos: [%f, %f, %f], vo pos: [%f, %f, %f]",
                   mocap_pos.x(), mocap_pos.y(), mocap_pos.z(),
                   baselink_tf_.getOrigin().x(), baselink_tf_.getOrigin().y(),
                   baselink_tf_.getOrigin().z());
          baselink_tf_.getBasis().getRPY(r, p, y);
          ROS_INFO("mocap yaw: %f, vo rot: [%f, %f, %f]", estimator_->getState(State::YAW_BASE, BasicEstimator::GROUND_TRUTH)[0], r, p, y);
        }
      estimateProcess(vo_msg->header.stamp);

      /* publish */
      vo_state_.header.stamp = vo_msg->header.stamp;

      for(int axis = 0; axis < 3; axis++)
        vo_state_.states[axis].state[0].x = baselink_tf_.getOrigin()[axis]; //raw
      vo_state_pub_.publish(vo_state_);

      /* update */
      updateHealthStamp(vo_msg->header.stamp.toSec());
    }

    void estimateProcess(ros::Time stamp)
    {
      /* YAW */
      /* TODO: the weighting filter with IMU */
      tfScalar r,p,y;
      baselink_tf_.getBasis().getRPY(r,p,y);
      estimator_->setState(State::YAW_BASE, BasicEstimator::EGOMOTION_ESTIMATE, 0, y);

      /* XYZ */
      for(auto& fuser : estimator_->getFuser(BasicEstimator::EGOMOTION_ESTIMATE))
        {
          string plugin_name = fuser.first;
          boost::shared_ptr<kf_plugin::KalmanFilter> kf = fuser.second;
          int id = kf->getId();

          if(plugin_name == "kalman_filter/kf_pos_vel_acc" ||
             plugin_name == "kalman_filter/kf_pos_vel_acc_bias")
            {
              /* set noise sigma */
              VectorXd measure_sigma(1);
              measure_sigma << vo_noise_sigma_;
              kf->setMeasureSigma(measure_sigma);

              /* correction */
              int index = id >> (State::X_BASE + 1);
              VectorXd meas(1); meas <<  baselink_tf_.getOrigin()[index];
              vector<double> params = {kf_plugin::POS};
              kf->correction(meas, params, stamp.toSec());

              VectorXd state = kf->getEstimateState();
              estimator_->setState(index + 3, BasicEstimator::EGOMOTION_ESTIMATE, 0, state(0));
              estimator_->setState(index + 3, BasicEstimator::EGOMOTION_ESTIMATE, 1, state(1));

              vo_state_.states[index].state[1].x = state(0); //estimated pos
              vo_state_.states[index].state[1].y = state(1); //estimated vel
            }
        }
    }

    void rosParamInit()
    {
      std::string ns = nhp_.getNamespace();

      nhp_.param("vio_flag", vio_flag_, true );
      if(param_verbose_) cout << ns << ": vio flag is " <<  vio_flag_ << endl;

      nhp_.param("valid_yaw", valid_yaw_, true );
      if(param_verbose_) cout << ns << ": valid yaw is " << valid_yaw_ << endl;

      nhp_.param("vo_noise_sigma", vo_noise_sigma_, 0.01 );
      if(param_verbose_) cout << ns << ": vo noise sigma is " <<  vo_noise_sigma_ << endl;

      nhp_.param("debug_verbose", debug_verbose_, false );
      if(param_verbose_) cout << ns << ": debug verbose is " <<  debug_verbose_ << endl;
    }
  };

};

/* plugin registration */
#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(sensor_plugin::VisualOdometry, sensor_plugin::SensorBase);



