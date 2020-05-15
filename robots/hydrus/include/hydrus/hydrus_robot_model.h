// -*- mode: c++ -*-
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2016, JSK Lab
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

#pragma once

#include <aerial_robot_model/transformable_aerial_robot_model.h>

class HydrusRobotModel : public aerial_robot_model::RobotModel {
public:
  HydrusRobotModel(bool init_with_rosparam,
                   bool verbose = false,
                   double fc_t_min_thre = 0,
                   double fc_rp_min_thre = 0,
                   double epsilon = 10,
                   int wrench_dof = 4);
  virtual ~HydrusRobotModel() = default;

  //public functions

  void calcFeasibleControlRollPitchDists();
  void calcFeasibleControlRollPitchDistsJacobian();

  virtual void calcWrenchMatrixOnRoot() override;
  virtual void calcStaticThrust() override;

  inline const uint8_t getWrenchDof() const { return wrench_dof_; }
  inline const double getRollPitchPositionMargin() const { return rp_position_margin_; }
  inline const double getRollPitchPositionMarginThresh() const { return rp_position_margin_thre_; }
  inline const double getWrenchMatDeterminant() const { return wrench_mat_det_; }
  inline const double getWrenchMatDetThresh() const {return wrench_mat_det_thre_;}
  const Eigen::MatrixXd& getFeasibleControlRollPitchDistsJacobian() const {return fc_rp_dists_jacobian_;}
  inline const double& getFeasibleControlRollPitchMin()  {return fc_rp_min_;}
  inline const double& getFeasibleControlRollPitchMinThre()  {return fc_rp_min_thre_;}
  inline const Eigen::VectorXd& getFeasibleControlRollPitchDists() const {return fc_rp_dists_;}
  inline const Eigen::VectorXd& getApproxFeasibleControlRollPitchDists() const {return approx_fc_rp_dists_;}

  bool rollPitchPositionMarginCheck(); // deprecated

  inline void setWrenchDof(uint8_t dof) { wrench_dof_ = dof; }
  virtual bool stabilityCheck(bool verbose = false) override;

  virtual void updateJacobians(const KDL::JntArray& joint_positions, bool update_model = true) override;

  bool wrenchMatrixDeterminantCheck(); // deprecated

private:

  // private attributes
  int wrench_dof_;

  Eigen::VectorXd approx_fc_rp_dists_;
  Eigen::VectorXd fc_rp_dists_;
  double fc_rp_min_;
  double fc_rp_min_thre_;
  Eigen::MatrixXd fc_rp_dists_jacobian_;

  // following variables will be replaced by fc_t_min, fc_f_min in the furture
  double wrench_mat_det_;
  double wrench_mat_det_thre_;
  double rp_position_margin_;
  double rp_position_margin_thre_;

  // private functions
  void getParamFromRos();

protected:

  void setFeasibleControlRollPitchDistsJacobian(const Eigen::MatrixXd fc_rp_dists_jacobian) {fc_rp_dists_jacobian_ = fc_rp_dists_jacobian;}

  virtual void updateRobotModelImpl(const KDL::JntArray& joint_positions) override;
};
