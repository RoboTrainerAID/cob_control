/*
 *****************************************************************
 * \file
 *
 * \note
 *   Copyright (c) 2015 \n
 *   Fraunhofer Institute for Manufacturing Engineering
 *   and Automation (IPA) \n\n
 *
 *****************************************************************
 *
 * \note
 *   Project name: care-o-bot
 * \note
 *   ROS stack name: cob_control
 * \note
 *   ROS package name: cob_obstacle_distance
 *
 * \author
 *   Author: Marco Bezzon, email: Marco.Bezzon@ipa.fraunhofer.de
 *
 * \date Date of creation: July, 2015
 *
 * \brief
 *   Implementation of the FrameToCollision management class.
 ****************************************************************/

#include <eigen_conversions/eigen_msg.h>
#include <visualization_msgs/Marker.h>

#include "cob_obstacle_distance/frame_to_collision.hpp"


FrameToCollision::FrameToCollision() : success_(true)
{

}


FrameToCollision::~FrameToCollision()
{

}


bool FrameToCollision::initFile(const std::string& root_frame_id, const std::string& urdf_file_name)
{
    this->root_frame_id_ = root_frame_id;
    this->success_ = this->model_.initFile(urdf_file_name);
    return this->success_;
}


bool FrameToCollision::initParameter(const std::string& root_frame_id, const std::string& urdf_param)
{
    this->root_frame_id_ = root_frame_id;
    this->success_ = this->model_.initParam(urdf_param);
    return this->success_;
}


bool FrameToCollision::getMarkerShapeFromUrdf(const Eigen::Vector3d& abs_pos,
                                              const Eigen::Quaterniond& quat_pos,
                                              const std::string& frame_of_interest,
                                              PtrIMarkerShape_t& segment_of_interest_marker_shape)
{
    if(!this->success_)
    {
        ROS_ERROR("FrameToCollision object has not been initialized correctly.");
        return false;
    }

    bool local_success = true;
    PtrLink_t link = this->model_.getLink(frame_of_interest);
    if(NULL != link)
    {
        geometry_msgs::Pose pose;
        tf::pointEigenToMsg(abs_pos, pose.position);
        tf::quaternionEigenToMsg(quat_pos, pose.orientation);

        std_msgs::ColorRGBA col;
        col.a = 1.0;
        col.r = 1.0;
        col.g = 0.0;
        col.b = 0.0;

        PtrCollision_t collision = link->collision;
        if(NULL != link->collision && NULL != link->collision->geometry)
        {
            this->createSpecificMarkerShape(frame_of_interest,
                                            pose,
                                            col,
                                            link->collision->geometry,
                                            segment_of_interest_marker_shape);
        }
        else
        {
            ROS_ERROR_STREAM("There is either no collision object or no collision geometry available: " << frame_of_interest);
            local_success = false;
        }
    }
    else
    {
        ROS_ERROR_STREAM("Could not find link in URDF model description: " << frame_of_interest);
        local_success = false;
    }

    return local_success;
}


void FrameToCollision::createSpecificMarkerShape(const std::string& frame_of_interest,
                                                 const geometry_msgs::Pose& pose,
                                                 const std_msgs::ColorRGBA& col,
                                                 const PtrGeometry_t& geometry,
                                                 PtrIMarkerShape_t& segment_of_interest_marker_shape)
{
    if(urdf::Geometry::MESH == geometry->type)
    {
        PtrMesh_t mesh = boost::static_pointer_cast<urdf::Mesh>(geometry);
        segment_of_interest_marker_shape.reset(new MarkerShape<BVH_RSS_t>(this->root_frame_id_,
                                                                          mesh->filename,
                                                                          pose,
                                                                          col));
    }
    else if(urdf::Geometry::BOX == geometry->type)
    {
        PtrBox_t urdf_box = boost::static_pointer_cast<urdf::Box>(geometry);
        fcl::Box b(urdf_box->dim.x,
                   urdf_box->dim.y,
                   urdf_box->dim.z);
        segment_of_interest_marker_shape.reset(new MarkerShape<fcl::Box>(this->root_frame_id_, b, pose, col));
    }
    else if(urdf::Geometry::SPHERE == geometry->type)
    {
        PtrSphere_t urdf_sphere = boost::static_pointer_cast<urdf::Sphere>(geometry);
        fcl::Sphere s(urdf_sphere->radius);
        segment_of_interest_marker_shape.reset(new MarkerShape<fcl::Sphere>(this->root_frame_id_, s, pose, col));
    }
    else if(urdf::Geometry::CYLINDER == geometry->type)
    {
        PtrCylinder_t urdf_cyl = boost::static_pointer_cast<urdf::Cylinder>(geometry);
        fcl::Cylinder c(urdf_cyl->radius, urdf_cyl->length);
        segment_of_interest_marker_shape.reset(new MarkerShape<fcl::Cylinder>(this->root_frame_id_, c, pose, col));
    }
    else
    {
        ROS_ERROR_STREAM("Geometry type unknown: " << geometry->type);
    }
}


bool FrameToCollision::getMarkerShapeFromType(const uint32_t& shape_type,
                                              const Eigen::Vector3d& abs_pos,
                                              const Eigen::Quaterniond& quat_pos,
                                              const std::string& frame_of_interest,
                                              PtrIMarkerShape_t& segment_of_interest_marker_shape)
{
    // Representation of segment_of_interest as specific fcl::Shape
    fcl::Box b(0.1, 0.1, 0.1);
    fcl::Sphere s(0.05);
    fcl::Cylinder c(0.05, 0.1);

    geometry_msgs::Pose pose;
    tf::pointEigenToMsg(abs_pos, pose.position);
    tf::quaternionEigenToMsg(quat_pos, pose.orientation);

    std_msgs::ColorRGBA col;
    col.a = 1.0;
    col.r = 1.0;
    col.g = 0.0;
    col.b = 0.0;

    uint32_t loc_shape_type = shape_type;
    std::string mesh_resource;
    if(visualization_msgs::Marker::MESH_RESOURCE == loc_shape_type)
    {
        PtrLink_t link = this->model_.getLink(frame_of_interest);
        if(this->success_ &&
                NULL != link &&
                NULL != link->collision &&
                NULL != link->collision->geometry &&
                link->collision->geometry->type == urdf::Geometry::MESH)
        {
            PtrMesh_t mesh = boost::static_pointer_cast<urdf::Mesh>(link->collision->geometry);
            mesh_resource = mesh->filename;
        }
        else
        {
            ROS_WARN_STREAM("Either link is not available in URDF or there is no MESH collision representation for " <<
                            frame_of_interest <<
                            ". Rather using SPHERE.");
            loc_shape_type = visualization_msgs::Marker::SPHERE;
        }
    }

    switch(loc_shape_type)
    {
        case visualization_msgs::Marker::CUBE:
            segment_of_interest_marker_shape.reset(new MarkerShape<fcl::Box>(this->root_frame_id_, b, pose, col));
            break;
        case visualization_msgs::Marker::SPHERE:
            segment_of_interest_marker_shape.reset(new MarkerShape<fcl::Sphere>(this->root_frame_id_, s, pose, col));
            break;
        case visualization_msgs::Marker::CYLINDER:
            segment_of_interest_marker_shape.reset(new MarkerShape<fcl::Cylinder>(this->root_frame_id_, c, pose, col));
            break;
        case visualization_msgs::Marker::MESH_RESOURCE:
            segment_of_interest_marker_shape.reset(new MarkerShape<BVH_RSS_t>(this->root_frame_id_,
                                                                              mesh_resource,
                                                                              pose,
                                                                              col));
            break;
        default:
           ROS_ERROR("Failed to process request due to unknown shape type: %d", loc_shape_type);
           return false;
    }

    return true;

}
