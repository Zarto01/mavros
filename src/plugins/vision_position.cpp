/**
 * @brief VisionPosition plugin
 * @file vision_position.cpp
 * @author M.H.Kabir <mhkabir98@gmail.com>
 * @author Vladimir Ermakov <vooon341@gmail.com>
 *
 * @addtogroup plugin
 * @{
 */
/*
 * Copyright 2014 M.H.Kabir.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <mavros/mavros_plugin.h>
#include <pluginlib/class_list_macros.h>

#include <tf/transform_listener.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/PoseWithCovarianceStamped.h>

namespace mavplugin {

/**
 * @brief Vision position plugin
 *
 * Send position estimation from various vision estimators
 * to FCU position controller.
 */
class VisionPositionPlugin : public MavRosPlugin {
public:
	VisionPositionPlugin()
	{ };

	void initialize(UAS &uas_,
			ros::NodeHandle &nh,
			diagnostic_updater::Updater &diag_updater)
	{
		bool pose_with_covariance;

		uas = &uas_;

		nh.param("position/vision_pose_with_covariance", pose_with_covariance, false);
		ROS_DEBUG_STREAM_NAMED("position", "Vision position topic type: " <<
				(pose_with_covariance)? "PoseWithCovarianceStamped" : "PoseStamped");

		if (pose_with_covariance)
			vision_sub = nh.subscribe("position/vision", 10, &VisionPositionPlugin::vision_cov_cb, this);
		else
			vision_sub = nh.subscribe("position/vision", 10, &VisionPositionPlugin::vision_cb, this);
	}

	const std::string get_name() const {
		return "VisionPosition";
	}

	const std::vector<uint8_t> get_supported_messages() const {
		return { /* Rx disabled */ };
	}

	void message_rx_cb(const mavlink_message_t *msg, uint8_t sysid, uint8_t compid) {
	}

private:
	UAS *uas;

	ros::Subscriber vision_sub;

	/* -*- low-level send -*- */

	void vision_position_estimate(uint64_t usec,
			float x, float y, float z,
			float roll, float pitch, float yaw) {
		mavlink_message_t msg;
		mavlink_msg_vision_position_estimate_pack_chan(UAS_PACK_CHAN(uas), &msg,
				usec,
				x,
				y,
				z,
				roll,
				pitch,
				yaw);
		uas->mav_link->send_message(&msg);
	}

	/* -*- mid-level helpers -*- */

	/**
	 * Send vision estimate transform to FCU position controller
	 */
	void send_vision_transform(const tf::Transform &transform, const ros::Time &stamp) {
		// origin and RPY in ENU frame
		tf::Vector3 position = transform.getOrigin();
		double roll, pitch, yaw;
		tf::Matrix3x3 orientation(transform.getBasis());
		orientation.getRPY(roll, pitch, yaw);

		// TODO: check conversion. Issue #49.
		vision_position_estimate(stamp.toNSec() / 1000,
				position.y(), position.x(), -position.z(),
				roll, -pitch, -yaw); // ??? please check!
	}

	/* -*- callbacks -*- */

	/* TODO: tf listener */

	void vision_cov_cb(const geometry_msgs::PoseWithCovarianceStamped::ConstPtr &req) {
		tf::Transform transform;
		poseMsgToTF(req->pose.pose, transform);
		send_vision_transform(transform, req->header.stamp);
	}

	void vision_cb(const geometry_msgs::PoseStamped::ConstPtr &req) {
		tf::Transform transform;
		poseMsgToTF(req->pose, transform);
		send_vision_transform(transform, req->header.stamp);
	}
};

}; // namespace mavplugin

PLUGINLIB_EXPORT_CLASS(mavplugin::VisionPositionPlugin, mavplugin::MavRosPlugin)
