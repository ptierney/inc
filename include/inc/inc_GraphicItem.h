
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

// GraphicItems do not have positions, rotations, or scales
// those are determined by the physics engine

#include <BulletSoftBody/btSoftBody.h>

#include <cinder/Vector.h>
#include <cinder/gl/Vbo.h>
#include <cinder/Ray.h>

namespace inc {
class Exporter;

class GraphicItem {
public:
    GraphicItem();
    virtual ~GraphicItem(); 
    virtual void draw() = 0;

    // maybe should be pure virtual...?
    virtual void save(Exporter&) { }

    void set_visible(bool);
    bool visible();

private:
    bool visible_;
};

typedef std::tr1::shared_ptr<GraphicItem> GraphicItemPtr;

class Solid;

class SolidGraphicItem : public GraphicItem {
public:
    SolidGraphicItem();
    virtual ~SolidGraphicItem();

    Solid& solid();
    void set_solid(Solid*);

    virtual bool has_alternate_bounding_sphere() { return false; }
    virtual float bounding_sphere_radius() { return 0.0f; }

    virtual bool detect_selection(ci::Ray) { return false; }

    bool flip_normals_;

private:
    Solid* solid_;
};


class OriginGraphicItem : public GraphicItem {
public:
    OriginGraphicItem();
    virtual ~OriginGraphicItem() { }

    virtual void draw();

    float* grid_plane_size_ptr();
    float* grid_plane_intervals_ptr();

private:
    void draw_axis();
    void draw_grid_plane_lines();

    float grid_plane_size_;
    float grid_plane_intervals_;
};


class BoxGraphicItem : public SolidGraphicItem {
public:
    BoxGraphicItem(ci::Vec3f dimensions);
    virtual ~BoxGraphicItem() { }

    virtual void draw();

private:
    ci::Vec3f dimensions_;
};


class PlaneGraphicItem : public SolidGraphicItem {
public:
    PlaneGraphicItem(ci::Vec3f dimensions);
    virtual ~PlaneGraphicItem() { }

    virtual void draw();

private:
    ci::Vec3f dimensions_;
};


class VboGraphicItem : public SolidGraphicItem {
public:
    VboGraphicItem(ci::gl::VboMesh&, ci::Vec3f scale);
    virtual ~VboGraphicItem() { }

    virtual void draw();

private:
    ci::gl::VboMesh vbo_mesh_;
    ci::Vec3f scale_;
};


class SphereGraphicItem : public SolidGraphicItem {
public:
    SphereGraphicItem(float radius);
    virtual ~SphereGraphicItem();

    virtual void draw();

    virtual bool has_alternate_bounding_sphere() { return true; }
    virtual float bounding_sphere_radius();

private:
    float radius_;
};


class SoftBodyGraphicItem : public SolidGraphicItem {
public:
    SoftBodyGraphicItem(btSoftBody* soft_body, ci::ColorA color);
    virtual ~SoftBodyGraphicItem();

    virtual void draw();

    // check the ray intersection with all the triangles of the mesh
    virtual bool detect_selection(ci::Ray);

    static bool draw_face_normals_;
    static float face_normals_length_;
    static ci::ColorA face_normals_color_;

private:
    ci::Vec3f get_face_center(int index) {
        return ci::Vec3f(
            (soft_body_->m_faces[index].m_n[0]->m_x.x() + 
            soft_body_->m_faces[index].m_n[1]->m_x.x() + 
            soft_body_->m_faces[index].m_n[2]->m_x.x()) / 3.0f,

            (soft_body_->m_faces[index].m_n[0]->m_x.y() + 
            soft_body_->m_faces[index].m_n[1]->m_x.y() + 
            soft_body_->m_faces[index].m_n[2]->m_x.y()) / 3.0f,

            (soft_body_->m_faces[index].m_n[0]->m_x.z() + 
            soft_body_->m_faces[index].m_n[1]->m_x.z() + 
            soft_body_->m_faces[index].m_n[2]->m_x.z()) / 3.0f);
    }

    void make_gl_normal(int face, int node) {
        if (flip_normals_) {
            glNormal3f(
                -soft_body_->m_faces[face].m_n[node]->m_n.x(),
                -soft_body_->m_faces[face].m_n[node]->m_n.y(),
                -soft_body_->m_faces[face].m_n[node]->m_n.z());
        } else { 
            glNormal3f(
                soft_body_->m_faces[face].m_n[node]->m_n.x(),
                soft_body_->m_faces[face].m_n[node]->m_n.y(),
                soft_body_->m_faces[face].m_n[node]->m_n.z());
        }
    }

    void make_gl_vertex(int face, int node) {
        glVertex3f(
            soft_body_->m_faces[face].m_n[node]->m_x.x(),
            soft_body_->m_faces[face].m_n[node]->m_x.y(),
            soft_body_->m_faces[face].m_n[node]->m_x.z());
    }

    btSoftBody* soft_body_;
    ci::ColorA color_;

    float get_vertex_height(int face, int node) { 
        return soft_body_->m_faces[face].m_n[node]->m_x.y(); 
    }

    float last_min_y_;
    float last_max_y_;
};

class Exporter;

// TODO: create smooth mesh, with per vertex normals

class ShadedMesh : public GraphicItem {
public:
    ShadedMesh(const ci::TriMesh& mesh);
    ShadedMesh(std::shared_ptr<ci::TriMesh> mesh);

    // Override
    virtual void save(Exporter&);

    void draw();

    void set_color(const ci::ColorA& c);

    void set_shade(bool s) { shade_ = s; }
    void set_draw_wireframe(bool w) { draw_wireframe_ = w; }
    void set_save(bool s) { save_ = s; }

    // flips the directions of the normals
    void flip();

private:
    void init();
    void build_normals();

    ci::TriMesh mesh_;
    std::vector<ci::Vec3f> normals_;

    ci::ColorA color_;

    bool shade_;
    bool draw_wireframe_;
    bool save_;

    ci::Vec3f a_, b_, c_;

};

}