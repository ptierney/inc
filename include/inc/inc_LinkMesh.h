
/*  Copyright (c) 2010, Patrick Tierney
 *
 *  This file is part of INC (INC's Not CAD).
 *
 *  INC is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  INC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with INC.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>
#include <deque>

#include <inc/inc_GraphicItem.h>
#include <inc/inc_LinkFactory.h>
#include <inc/inc_Solid.h>
#include <inc/inc_Color.h>

class btTypedConstraint;
class btHingeConstraint;
class btPoint2PointConstraint;

namespace inc {

class Joint {
public:
    // this should be pure virtual 
    virtual ci::Vec3f a_position() = 0;
    virtual ci::Vec3f b_position() = 0;
    virtual ci::Vec3f position() = 0;
    virtual btTypedConstraint* constraint_ptr() = 0;

    virtual ~Joint() { }
};

typedef std::tr1::shared_ptr<Joint> JointPtr;

class HingeJoint : public Joint {
public:
    HingeJoint(btHingeConstraint* hinge)
        : hinge_(hinge) { }

    ci::Vec3f a_position();
    ci::Vec3f b_position();
    ci::Vec3f position();

    btTypedConstraint* constraint_ptr() { return hinge_; }
    btHingeConstraint* hinge_ptr() { return hinge_; }

private:
    btHingeConstraint* hinge_;
};

typedef std::tr1::shared_ptr<HingeJoint> HingeJointPtr;

class SocketJoint : public Joint {
public:
    SocketJoint(btPoint2PointConstraint* socket) 
        : socket_(socket) { }

    ci::Vec3f a_position();
    ci::Vec3f b_position();
    ci::Vec3f position();

    btTypedConstraint* constraint_ptr() { return socket_; }
    btPoint2PointConstraint* socket_ptr() { return socket_; }

private:
    btPoint2PointConstraint* socket_;
};

typedef std::tr1::shared_ptr<SocketJoint> SocketJointPtr;

class Exporter;

/* a link cell is an object needed more for rendering purposes 
 * a cell forms 6 triangles as follows:
 *  depth =>
 *  
 *  solid_1 ------- joints_[0] --- solid_2
 *      |        /     |      \      | 
 *      |       /      |       \     | 
 *  joints_[3]         |         joints_[1]
 *      |       \      |       /     | 
 *      |        \     |      /      | 
 *  solid_4 ------ joints_[2] ----- solid_3
 *  
 *  width
 *   ||
 *   \/
 * Lines designate OpenGL triangles
 */

class JointCell {
public:
    JointCell(std::vector<JointPtr> joints) 
        : joints_(joints) {
        color_ = ci::ColorA(0.9f, 0.9f, 0.9f, 1.0f);
        for (int i = 0; i < 6; ++i) {
            triangles_.push_back(std::vector<ci::Vec3f>());
            triangles_[i].resize(3);
        }
        normals_.resize(6);
    }

    std::vector<JointPtr> joints_;

    ci::ColorA color_;

    std::vector<std::vector<ci::Vec3f>> triangles_;
    std::vector<ci::Vec3f> normals_;

    void calculate_triangles();

    void draw();

    void save(Exporter& exporter);
};

typedef std::shared_ptr<JointCell> JointCellPtr;

class LinkMesh : public GraphicItem {
public:
    LinkMesh(int w, int d, LinkFactory::LinkType,
        std::shared_ptr<std::deque<RigidSolidPtr>> solids);
    // form a link mesh with hinges oriented according to vectors
    LinkMesh(int w, int d, std::shared_ptr<std::deque<RigidSolidPtr>> 
        solids, std::vector<std::vector<ci::Vec3f>> axis_w,
        std::vector<std::vector<ci::Vec3f>> axis_d);

    void isolate_hinges(); // parse throught joints_ and places hinges in hinge_joints_

    virtual ~LinkMesh();

    static std::shared_ptr<LinkMesh> create_link_mesh(int w, int d,
        float radius, float spacing_scale, LinkFactory::LinkType);
    static std::shared_ptr<LinkMesh> create_from_images(const std::string& file_1,
        const std::string& file_2, float sphere_radius, float spacing_scale);

    virtual void draw();

    // Override
    void save(Exporter&);

    static int new_mesh_w_;
    static int new_mesh_d_;
    static float new_mesh_height_;
    static ci::Vec3f hinge_axis_;
    static float line_weight_;

    void activate();

    std::shared_ptr<std::vector<HingeJointPtr>> hinge_joints_;

private:
    static std::shared_ptr<std::deque<RigidSolidPtr>> create_mesh_solids(
        int w, int d, float sphere_rad, float spacing, ci::Vec3f origin);


    std::vector<RigidSolidPtr> solids_;
    std::shared_ptr<std::vector<JointPtr>> joints_;
    std::shared_ptr<std::vector<JointCellPtr>>  joint_cells_;
    int w_;
    int d_;
};


}