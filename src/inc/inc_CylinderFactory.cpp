
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

#include <cinder/gl/gl.h>

#include <inc/inc_CylinderFactory.h>
#include <inc/inc_Solid.h>
#include <inc/inc_MeshCreator.h>
#include <inc/inc_Manager.h>

namespace inc {

CylinderFactory* CylinderFactory::instance_;

CylinderFactory::CylinderFactory() {
    instance_ = this;
}

CylinderFactory::~CylinderFactory() {
    // Nothing here
}

SoftSolidPtr CylinderFactory::create_soft_cylinder(std::pair<ci::Vec3f, 
    ci::Vec3f> centers, float radius, int resolution) {
    // TODO: hook into slice and rotate res
    /*
    TriMeshPtr mesh = MeshCreator::instance().generate_bspline_revolve_mesh(
        generate_cylinder_bspline(centers, radius), 20, 40);

    SoftSolidPtr ptr = SolidFactory::create_soft_mesh(mesh, ci::Vec3f::one(),
        false);
    */

    std::tr1::shared_ptr<std::vector<ci::Vec3f>> points = 
        MeshCreator::instance().generate_bspline_revolve_points(
        generate_cylinder_bspline(centers, radius), 20, 40);

    SoftSolidPtr ptr = 
        SolidFactory::instance().create_soft_container_from_convex_hull(points);

    Manager::instance().add_solid(ptr);

    return ptr;
}

SoftSolidPtr CylinderFactory::create_soft_cylinder_network(std::vector<
    std::pair<ci::Vec3f, ci::Vec3f>>, float radius, int resolution) {
    
    return SolidFactory::create_soft_sphere(ci::Vec3f(), ci::Vec3f::one());
}

std::tr1::shared_ptr<ci::BSpline3f> CylinderFactory::generate_cylinder_bspline(std::pair<
    ci::Vec3f, ci::Vec3f> centers, float radius) {
    std::vector<ci::Vec3f> points;

    // number of control points per disc
    int num_disc = 5;
    // number of control points for the side length
    int num_side = 10;

    ci::Vec3f start = centers.first;
    ci::Vec3f end = centers.second;
    ci::Vec3f axis = end - start;
    axis.normalize();
    ci::Vec3f alt = ci::Vec3f::yAxis();
    if (alt == axis)
        alt = ci::Vec3f::zAxis();
    // perp is the vector lying in the circles
    ci::Vec3f perp = axis.cross(alt);
    perp.normalize();

    // the current working point
    ci::Vec3f point = start;

    float disc_step = radius / (float) num_disc;
    float side_step = (end - start).length() / (float) num_side;

    // walk along the entire perimeter of the line, adding points
    // start with the center of the first disc
    points.push_back(point);

    // add points for the start disc
    for (int i = 0; i < num_disc; ++i) {
        point += perp * disc_step;
        points.push_back(point);
    }

    // add points for the side
    for (int i = 0; i < num_side; ++i) {
        point += axis * side_step;
        points.push_back(point);
    }

    // walk backwards on the top disc
    for (int i = 0; i < num_disc; ++i) {
        point -= perp * disc_step;
        points.push_back(point);
    }

    debug_spline_ = std::tr1::shared_ptr<ci::BSpline3f>(
        new ci::BSpline3f(points, 3, false, true));

    // 1st = points, 2nd = degree, 3rd = add points to close, 4th = is it open
    return std::tr1::shared_ptr<ci::BSpline3f>(new ci::BSpline3f(points, 3,
        false, true));
}

void CylinderFactory::draw() {
    if (debug_spline_.get() == NULL)
        return;

    glBegin(GL_LINE_STRIP);

    for (float t = 0.0f; t <= 1.0f; t += 0.01) {
        ci::gl::vertex(debug_spline_->getPosition(t));
    }

    glEnd();
}

CylinderFactory& CylinderFactory::instance() {
    return *instance_;
}

}
