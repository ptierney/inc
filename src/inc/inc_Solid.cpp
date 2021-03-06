
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

#include <math.h>

#include <deque>
#include <map>
#include <algorithm>

#include <LinearMath/btIDebugDraw.h>
#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>

// Required for CinderBullet.h
#include <cinder/gl/gl.h>
#include <cinder/app/AppBasic.h>
#include <cinder/Surface.h>
#include <cinder/gl/Material.h>
#include <cinder/gl/Light.h>
#include <cinder/Camera.h>
#include <cinder/AxisAlignedBox.h>
#include <cinder/Sphere.h>
#include <cinder/gl/GlslProg.h>
// N.B. CinderBullet.h is more like a .cpp, in can only be included once in a project
#include <blocks/bullet/CinderBullet.h>
#include <cinder/Vector.h>
#include <cinder/Quaternion.h>
#include <cinder/app/MouseEvent.h>
#include <cinder/Matrix.h>
#include <cinder/TriMesh.h>
#include <cinder/Rand.h>
#include <cinder/ObjLoader.h>

#include <inc/inc_Solid.h>
#include <inc/inc_GraphicItem.h>
#include <inc/inc_Manager.h>
#include <inc/BunnyMesh.h>
#include <inc/inc_DxfSaver.h>
#include <inc/inc_Menu.h>
#include <inc/inc_MeshCreator.h>
#include <inc/inc_Renderer.h>
#include <inc/inc_Color.h>

namespace inc {

bool Solid::allow_forces_;
bool Solid::allow_selection_;

Solid::Solid(SolidGraphicItem* item, btCollisionObject* body, btDynamicsWorld* world) 
    : graphic_item_(item), body_(body), world_(world) {
    selected_ = false;
    has_force_ = false;
    visible_ = true;
    force_ = ci::Vec3f::zero();

    // it is still valid to have a solid without a graphic representation
    if (item != NULL)
        graphic_item_->set_solid(this);
}

Solid::~Solid() {
#ifdef TRACE_DTORS
    ci::app::console() << "Deleting Solid" << std::endl;
#endif
    if (graphic_item_)
        delete graphic_item_;

    delete body_;
}

void Solid::update() {
    // nothing here
}

void Solid::draw() {
    if (graphic_item_ != NULL)
        graphic_item_->draw();
}

btCollisionObject& Solid::collision_object() {
    return *body_;
}

void Solid::remove_force() {
    has_force_ = false;
}

void Solid::set_visible(bool vis) {
    visible_ = vis;
}

bool Solid::visible() {
    return visible_;
}

/* Original code from: http://www.devmaster.net/wiki/Ray-sphere_intersection
float intersectRaySphere(const Ray &ray, const Sphere &sphere) {
	Vec dst = ray.o - sphere.o;
	float B = dot(dst, ray.d);
	float C = dot(dst, dst) - sphere.r2;
	float D = B*B - C;
	return D > 0 ? -B - sqrt(D) : std::numeric_limits<float>::infinity();
}
*/
bool Solid::detect_selection(ci::Ray r) {
    btVector3 center;
    float radius;

    // note about radius: the radius is calculated from the bounding box,
    // not a least-fit calculation around the object. Therefore, the 
    // bounding sphere of a sphere-shaped object is the sphere that 
    // contains the bounding box of the sphere (not just the sphere)
    body_->getCollisionShape()->getBoundingSphere(center, radius);
    center += body_->getWorldTransform().getOrigin();

    // override the bullet sphere if possible
    if (graphic_item_->has_alternate_bounding_sphere())
        radius = graphic_item_->bounding_sphere_radius();

    ci::Vec3f dst = r.getOrigin() - ci::Vec3f(center.x(), center.y(), center.z());
    float B = dst.dot(r.getDirection());
    float C = dst.dot(dst) - radius * radius;
    float D = B * B - C;

    return D > 0;
}

// the force menu hooks into this
ci::Vec3f* Solid::force_ptr() {
    return &force_;
}

void Solid::select() {
    selected_ = !selected_;
}

bool Solid::selected() {
    return selected_;
}

void Solid::set_selected(bool s) {
    selected_ = s;
}


RigidSolid::RigidSolid(SolidGraphicItem* item, btRigidBody* body, btDynamicsWorld* world) 
    : Solid(item, body, world) {
}

RigidSolid::~RigidSolid() {
    if (rigid_body().getMotionState())
        delete rigid_body().getMotionState();

    if (rigid_body().getCollisionShape())
        delete rigid_body().getCollisionShape();

    world_->removeRigidBody(rigid_body_ptr());
}

void RigidSolid::draw() {
    ci::Matrix44f tf = ci::bullet::getWorldTransform(rigid_body_ptr());

    glPushMatrix();
        glMultMatrixf(tf.m);
        Solid::draw();
    glPopMatrix();
}

// saving rigid solids is not supported at the moment
void RigidSolid::save(Exporter&) {
    // nothing here
}

void RigidSolid::set_gravity(float g) {
    rigid_body().setGravity(btVector3(0.0, g, 0.0));
    body_->activate();
}

void RigidSolid::set_force(ci::Vec3f) {
    // nothing here, might want to fold in force_ and has_force_ into
    // Solid class if this ever gets used
}

btRigidBody& RigidSolid::rigid_body() {
    return *(btRigidBody::upcast(body_));
}

btRigidBody* RigidSolid::rigid_body_ptr() {
    return btRigidBody::upcast(body_);
}

ci::Vec3f RigidSolid::position() {
    btVector3 origin = rigid_body().getWorldTransform().getOrigin();

    return ci::Vec3f(origin.x(), origin.y(), origin.z());
}




SoftSolid::SoftSolid(SolidGraphicItem* item, btSoftBody* body, btDynamicsWorld* world) 
    : Solid(item, body, world) {
}

SoftSolid::~SoftSolid() {
    SolidFactory::instance().soft_dynamics_world()->removeSoftBody(
        soft_body_ptr());
}

void SoftSolid::draw() {
    Solid::draw();
}

void SoftSolid::save(Exporter& exporter) {
    exporter.input_soft_solid(*this);
}

// TODO: actually set gravity
void SoftSolid::set_gravity(float g) {
    body_->activate();
}

void SoftSolid::set_force(ci::Vec3f vel) {
    has_force_ = true;
    force_ = vel;
}

void SoftSolid::update() {
    if (!has_force_) 
        return;

    soft_body().setVelocity(ci::bullet::toBulletVector3(force_));
    // check the timer, then 
}

btSoftBody& SoftSolid::soft_body() {
    return *(btSoftBody::upcast(body_));
}

btSoftBody* SoftSolid::soft_body_ptr() {
    return btSoftBody::upcast(body_);
}

bool SoftSolid::detect_selection(ci::Ray r) {
    return graphic_item_->detect_selection(r);
}

void SoftSolid::set_selected(bool s) {
    if (s)
        select();
    else
        Solid::set_selected(s);
}

void SoftSolid::select() {
    if (!allow_selection_)
        return;
    
    Solid::select();

    if (selected()) {
        if (allow_forces_)
            ForceMenu::add_menu(*this);
    } else {
        ForceMenu::remove_menu();
    }
}

std::shared_ptr<ci::TriMesh> SoftSolid::get_mesh() {
    btSoftBody& sb = soft_body();

    int num_faces = sb.m_faces.size();

    std::shared_ptr<ci::TriMesh> mesh_ptr(new ci::TriMesh());

    for (int i = 0; i < num_faces; ++i) {
        mesh_ptr->appendVertex(ci::Vec3f(
            sb.m_faces[i].m_n[0]->m_x.x(),
            sb.m_faces[i].m_n[0]->m_x.y(),
            sb.m_faces[i].m_n[0]->m_x.z()));

        mesh_ptr->appendVertex(ci::Vec3f(
            sb.m_faces[i].m_n[1]->m_x.x(),
            sb.m_faces[i].m_n[1]->m_x.y(),
            sb.m_faces[i].m_n[1]->m_x.z()));

        mesh_ptr->appendVertex(ci::Vec3f(
            sb.m_faces[i].m_n[2]->m_x.x(),
            sb.m_faces[i].m_n[2]->m_x.y(),
            sb.m_faces[i].m_n[2]->m_x.z()));

        mesh_ptr->appendTriangle(i * 3, i * 3 + 1, i * 3 + 2);
    }

    return mesh_ptr;
}


std::deque<btTriangleMesh*> SolidFactory::mesh_cleanup_;
ci::ColorA SolidFactory::sphere_color_;
ci::ColorA SolidFactory::container_color_;

SolidFactory::SolidFactory() {
    instance_ = this;
    gravity_ = 1.1f;//-9.8f;//0.0f;//-9.8f;
    last_gravity_ = gravity_;
    sphere_color_ = ci::ColorA(0.f ,0.4549f, 0.6275f, 0.45f);
    container_color_ = ci::ColorA(1.f, 1.f, 1.f, 0.45f);
    Solid::allow_forces_ = false;
    Solid::allow_selection_ = false;

    kDF_ = 1;
	kDP_ = 1.0f;
    kDG_ = 1.0f;
	kPR_ = 0.0f;
    kMT_ = 0.75f;

    sphere_kLST_ = 0.1;
    sphere_kVST_ = 0.1;
    sphere_kDF_ = 1;
    sphere_kDP_ = 0.001;
    sphere_kPR_ = 2500;
    sphere_total_mass_ = 1000.0f;

    draw_bullet_debug_ = false;
}

void SolidFactory::setup() {
    init_physics();
}

void SolidFactory::init_physics() {
    collision_configuration_ = new btSoftBodyRigidBodyCollisionConfiguration();
    int max_proxies = 32766;
    btVector3 world_aabb_min(-300,-300,-300);
	btVector3 world_aabb_max(300,300,300);
    broadphase_ = new btAxisSweep3(world_aabb_min, world_aabb_max, max_proxies);
    soft_body_world_info_.m_broadphase = broadphase_;

    dispatcher_ = new btCollisionDispatcher(collision_configuration_);
    soft_body_world_info_.m_dispatcher = dispatcher_;

    solver_ = new btSequentialImpulseConstraintSolver();

    dynamics_world_ = new btSoftRigidDynamicsWorld(dispatcher_, broadphase_, 
        solver_, collision_configuration_);

    dynamics_world_->setGravity(btVector3(0, gravity_, 0));
    soft_body_world_info_.m_gravity = btVector3(0, gravity_, 0);

    soft_body_world_info_.m_sparsesdf.Initialize();

    debug_draw_ = new DebugDraw();
    debug_draw_->setDebugMode(
        btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawConstraints);
        //btIDebugDraw::DBG_DrawAabb);
    dynamics_world_->setDebugDrawer(debug_draw_);
}

void SolidFactory::update() {
    if (gravity_ != last_gravity_) {
        dynamics_world_->setGravity(btVector3(0, gravity_, 0));
        soft_body_world_info_.m_gravity = btVector3(0, gravity_, 0);
        update_object_gravity();

        last_gravity_ = gravity_;
    }

    dynamics_world_->stepSimulation(1.0f, 10);
	
    double now = ci::app::getElapsedSeconds();
	double time_step_ = now - last_time_;
	last_time_ = now;
}

void SolidFactory::draw() {
    if (draw_bullet_debug_) {
        Renderer::set_line_width(0.9f);
        glBegin(GL_LINES);
        dynamics_world_->debugDrawWorld();
        glEnd();
    }
}

SolidFactory::~SolidFactory() {
#ifdef TRACE_DTORS
    ci::app::console() << "Deleting SolidFactory" << std::endl;
#endif
        
    // all the bullet bodies should be deleted by now
    std::for_each(mesh_cleanup_.begin(), mesh_cleanup_.end(),
        [] (btTriangleMesh* ptr) { delete ptr; } );

    soft_body_world_info_.m_sparsesdf.Reset();

    delete debug_draw_;
    delete dynamics_world_;
    delete solver_;
    delete dispatcher_;
    delete broadphase_;
    delete collision_configuration_;
}

void SolidFactory::delete_constraints() {
    while (dynamics_world_->getNumConstraints()) {
		btTypedConstraint* pc = dynamics_world_->getConstraint(0);
		dynamics_world_->removeConstraint(pc);
		delete pc;
	}
}

SolidPtr SolidFactory::create_solid_box(ci::Vec3f dimensions, 
    ci::Vec3f position) {
    btRigidBody* body = ci::bullet::createBox(SolidFactory::instance().dynamics_world(), 
        dimensions, ci::Quatf(), position);

    SolidGraphicItem* item = new BoxGraphicItem(dimensions);

    SolidPtr solid(new RigidSolid(item, body, 
        SolidFactory::instance().dynamics_world()));

    return solid;
}

SolidPtr SolidFactory::create_rigid_mesh(ci::TriMesh& mesh, 
    ci::Vec3f position, ci::Vec3f scale, float mass) {
    // Create bullet object
    btConvexHullShape* shape = ci::bullet::createConvexHullShape(mesh, scale);

    btRigidBody* body = ci::bullet::createConvexHullBody(
        SolidFactory::instance().dynamics_world(), shape, position, mass);

    // Rotate 90 degrees
    btQuaternion quat;
    quat.setRotation(btVector3(1, 0, 0), PI/2.0);
    btTransform trans(quat);
    body->setCenterOfMassTransform(trans);

    // Create GraphicItem object
    ci::gl::VboMesh vbo_mesh(mesh);
    SolidGraphicItem* item = new VboGraphicItem(vbo_mesh, scale);

    SolidPtr solid(new RigidSolid(item, body,
        SolidFactory::instance().dynamics_world()));

    return solid;
}

SolidPtr SolidFactory::create_plane(ci::Vec3f dimension,
    ci::Vec3f position) {
    // make a ground plane that cannot be moved
    btCollisionShape * groundShape	= new btStaticPlaneShape(
        btVector3(0,1,0),1);
	    
    btDefaultMotionState * groundMotionState = 
        new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1),
        btVector3(0,-1,0)));
    btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(
        0,groundMotionState,groundShape,btVector3(0,0,0));
	
    btRigidBody* body = new btRigidBody(groundRigidBodyCI);
    SolidFactory::instance().dynamics_world()->addRigidBody(body);

    SolidGraphicItem* item = new PlaneGraphicItem(dimension);

    SolidPtr solid(new RigidSolid(item, body, 
        SolidFactory::instance().dynamics_world()));

    return solid;
}

SolidPtr SolidFactory::create_static_solid_box(ci::Vec3f dimensions, 
    ci::Vec3f position) {
    btCollisionShape* box = new btBoxShape(
        ci::bullet::toBulletVector3(dimensions) / 2.0f);

	btDefaultMotionState* motion_state = 
        new btDefaultMotionState(
        btTransform(ci::bullet::toBulletQuaternion(ci::Quatf()),
        ci::bullet::toBulletVector3(position)));
		
	btVector3 inertia(0,0,0);
	float mass = 0.0f; // objects of mass 0 do not move
	box->calculateLocalInertia(mass, inertia);
	btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(mass, motion_state, 
        box, inertia);
		
	btRigidBody *rigid_body = new btRigidBody(rigidBodyCI);
	SolidFactory::instance().dynamics_world()->addRigidBody(rigid_body);

    //SolidGraphicItem* item = new BoxGraphicItem(dimensions);

    SolidPtr solid(new RigidSolid(NULL, rigid_body, 
        SolidFactory::instance().dynamics_world()));

    return solid;
}

RigidSolidPtr SolidFactory::create_rigid_sphere(ci::Vec3f position, ci::Vec3f radius) {
    btRigidBody* body = create_bullet_rigid_sphere(position, radius.x);

    RigidSolidPtr solid(new RigidSolid(new SphereGraphicItem(radius.x), body, 
        SolidFactory::instance().dynamics_world()));

    return solid;
}

float SolidFactory::kDF_;
float SolidFactory::kDP_;
float SolidFactory::kDG_;
float SolidFactory::kPR_;
float SolidFactory::kMT_;

float SolidFactory::sphere_kLST_;
float SolidFactory::sphere_kVST_;
float SolidFactory::sphere_kDF_;
float SolidFactory::sphere_kDP_;
float SolidFactory::sphere_kPR_;
float SolidFactory::sphere_total_mass_;

// NOTE: this not only loads the mesh, it locks the bottom vertices
SoftSolidPtr SolidFactory::create_soft_mesh(std::tr1::shared_ptr<ci::TriMesh> in_mesh,
    ci::Vec3f scl, bool lock_base_vertices) {
    ci::TriMesh mesh_copy = *in_mesh; // TODO: remove this copy step

    std::tr1::shared_ptr<ci::TriMesh> mesh_ptr = 
        remove_mesh_duplicates(mesh_copy);

    ci::TriMesh mesh; // mesh with duplicates removed
    mesh = *mesh_ptr;

    btScalar* vertices = new float[mesh.getNumVertices() * 3];

    int i = 0;
    for (std::vector<ci::Vec3f>::const_iterator it = mesh.getVertices().begin();
        it != mesh.getVertices().end(); ++it) { 
        vertices[i] = it->x; ++i;
        vertices[i] = it->y; ++i;
        vertices[i] = it->z; ++i;
    }

    int* triangles = new int[mesh.getNumIndices()];
        
    i = 0;
    for (std::vector<size_t>::const_iterator it = mesh.getIndices().begin();
        it != mesh.getIndices().end(); ++it) {
        triangles[i] = *it;
        ++i;
    }
        
    btSoftBody* soft_body = btSoftBodyHelpers::CreateFromTriMesh(
        SolidFactory::instance().soft_body_world_info(),
        vertices, triangles, mesh.getNumTriangles(), false);


    soft_body->m_materials[0]->m_kLST = 0.1;
    //soft_body->m_cfg.aeromodel = btSoftBody::eAeroModel::V_TwoSided;
	soft_body->m_cfg.kDF = kDF_;
	soft_body->m_cfg.kDP = kDP_; // no fun
    soft_body->m_cfg.kDG = kDG_; // no fun
	soft_body->m_cfg.kPR = kPR_;
    soft_body->m_cfg.kMT = kMT_; // pose rigiditiy

    soft_body->m_cfg.collisions |= btSoftBody::fCollision::VF_SS;
        
    btMatrix3x3 m;
    // This sets the axis, I think
    //m.setEulerZYX(-M_PI / 2.0f, 0.0, 0.0);
    m.setIdentity();
    // This sets the origin / starting position
    soft_body->scale(ci::bullet::toBulletVector3(scl));
    //soft_body->transform(btTransform(m, 
    //    ci::bullet::toBulletVector3(ci::Vec3f(0.0f, 50.0f, 0.0f))));
    
    for (int i = 0; i < soft_body->m_nodes.size(); ++i) {
        soft_body->setMass(i, 1.0f);
    }

    if (lock_base_vertices) {
        std::tr1::shared_ptr<std::vector<int> > anchors = get_top_vertices(mesh);

        std::for_each(anchors->begin(), anchors->end(),
            [soft_body] (int index) { soft_body->setMass(index, 0.0f); });
    }

    SolidFactory::instance().soft_dynamics_world()->addSoftBody(soft_body);

    SoftSolidPtr solid(new SoftSolid(
        new SoftBodyGraphicItem(soft_body, container_color_), 
        soft_body, SolidFactory::instance().dynamics_world()));

    delete [] triangles;
    delete [] vertices;
    
    return solid;
}

SoftSolidPtr SolidFactory::create_soft_container_from_convex_hull(
    std::tr1::shared_ptr<std::vector<ci::Vec3f>> points, bool lock_base_vertices) {

	btAlignedObjectArray<btVector3>	pts;

    std::for_each(points->begin(), points->end(), [&] (const ci::Vec3f& vec) {
        pts.push_back(btVector3(vec.x, vec.y, vec.z));
    } );

	btSoftBody* soft_body = btSoftBodyHelpers::CreateFromConvexHull(
        soft_body_world_info(),&pts[0],pts.size(), false);

    soft_body->m_materials[0]->m_kLST = 0.1;
    //soft_body->m_cfg.aeromodel = btSoftBody::eAeroModel::V_TwoSided;
	soft_body->m_cfg.kDF = kDF_;
	soft_body->m_cfg.kDP = kDP_; // no fun
    soft_body->m_cfg.kDG = kDG_; // no fun
	soft_body->m_cfg.kPR = kPR_;
    soft_body->m_cfg.kMT = kMT_; // pose rigiditiy

    for (int i = 0; i < soft_body->m_nodes.size(); ++i) {
        soft_body->setMass(i, 1.0f);
    }

    if (lock_base_vertices) {
        // create tri mesh from bullet soft body

        /*
        std::tr1::shared_ptr<std::vector<int> > anchors = get_top_vertices(mesh);

        std::for_each(anchors->begin(), anchors->end(),
            [soft_body] (int index) { soft_body->setMass(index, 0.0f); });
        */
    }

    soft_dynamics_world()->addSoftBody(soft_body);

    SoftSolidPtr solid(new SoftSolid(
        new SoftBodyGraphicItem(soft_body, container_color_), 
        soft_body, SolidFactory::instance().dynamics_world()));

    return solid;
}

SoftSolidPtr SolidFactory::create_soft_sphere(ci::Vec3f position, ci::Vec3f radius) {
    btSoftBody*	soft_body = create_bullet_soft_sphere(position, radius, 100);

    SoftSolidPtr solid(new SoftSolid(new SoftBodyGraphicItem(soft_body,
        sphere_color_), soft_body, 
        SolidFactory::instance().dynamics_world()));

    return solid;
}

std::tr1::shared_ptr<std::deque<SolidPtr> > SolidFactory::create_linked_soft_spheres(
    ci::Vec3f position, ci::Vec3f radius) {

    ci::Vec3f offset = ci::Vec3f(0.0f, radius.x * 1.1f, 0.0f);

    ci::Vec3f p1 = position + offset;
    ci::Vec3f p2 = position - offset;

    btSoftBody* sb1 = create_bullet_soft_sphere(
        p1, radius, 100);
    btSoftBody* sb2 = create_bullet_soft_sphere(
        p2, radius, 100);

    socket_link_soft_spheres(sb1, sb2, p1, p2);

    std::tr1::shared_ptr<std::deque<SolidPtr> > d_ptr = 
        std::tr1::shared_ptr<std::deque<SolidPtr> >(new std::deque<SolidPtr>());

    d_ptr->push_back(SolidPtr(new SoftSolid(
        new SoftBodyGraphicItem(sb1, sphere_color_), sb1, 
        SolidFactory::instance().dynamics_world())));
    d_ptr->push_back(SolidPtr(new SoftSolid(
        new SoftBodyGraphicItem(sb2, sphere_color_), sb2, 
        SolidFactory::instance().dynamics_world())));

    return d_ptr;
}

std::tr1::shared_ptr<std::deque<SolidPtr> > SolidFactory::create_soft_sphere_matrix(
    ci::Vec3f position, ci::Vec3f radius, int w, int h, int d) {

    std::vector<std::vector<std::vector<btSoftBody*> > > s_bodies;
    std::vector<std::vector<std::vector<ci::Vec3f> > > positions;

    s_bodies.resize(w);
    positions.resize(w);
    for (int i = 0; i < w; ++i) {
        s_bodies[i].resize(h);
        positions[i].resize(h);
        for (int j = 0; j < h; ++j) {
            s_bodies[i][j].resize(d);
            positions[i][j].resize(d);
        }
    }

    ci::Vec3f ptemp = position;
    float r = radius.x;
    float gap = r * 0.4f;
    ci::Vec3f xgap = ci::Vec3f(gap, 0.0f, 0.0f);
    ci::Vec3f ygap = ci::Vec3f(0.0f, gap, 0.0f);
    ci::Vec3f zgap = ci::Vec3f(0.0f, 0.0f, gap);
    ci::Vec3f xdiam = ci::Vec3f(r*2.0f, 0.0f, 0.0f);
    ci::Vec3f ydiam = ci::Vec3f(0.0f, r*2.0f, 0.0f);
    ci::Vec3f zdiam = ci::Vec3f(0.0f, 0.0f, r*2.0f);
    int resolution = 50;

    
    if (MeshCreator::instance().is_pointed_up()) {
        ygap *= -1.0f;
        ydiam *= -1.0f;
    }

    std::tr1::shared_ptr<std::deque<SolidPtr> > d_ptr = 
        std::tr1::shared_ptr<std::deque<SolidPtr> >(new std::deque<SolidPtr>());

    for (int i = 0; i < w; ++i) {
        for (int j = 0; j < h; ++j) {
            for (int k = 0; k < d; ++k) {
                ci::Vec3f p = position + xgap * i + ygap * j + zgap * k +
                    xdiam * i + ydiam * j + zdiam * k;
                positions[i][j][k] = p;
                s_bodies[i][j][k] = create_bullet_soft_sphere(p, radius,
                    resolution);

                d_ptr->push_back(SolidPtr(new SoftSolid(
                    new SoftBodyGraphicItem(s_bodies[i][j][k],sphere_color_), 
                    s_bodies[i][j][k], SolidFactory::instance().dynamics_world())));
            }
        }
    }

    /*
    for (int i = 0; i < w; ++i) {
        for (int j = 0; j < h; ++j) {
            for (int k = 0; k < d; ++k) {
                if (i > 0) {
                    socket_link_soft_spheres(
                        s_bodies[i-1][j][k],
                        s_bodies[i][j][k],
                        positions[i-1][j][k],
                        positions[i][j][k]);
                }

                if (j > 0) {
                    socket_link_soft_spheres(
                        s_bodies[i][j-1][k],
                        s_bodies[i][j][k],
                        positions[i][j-1][k],
                        positions[i][j][k]);
                }

                if (k > 0) {
                    socket_link_soft_spheres(
                        s_bodies[i][j][k-1],
                        s_bodies[i][j][k],
                        positions[i][j][k-1],
                        positions[i][j][k]);
                }
            }
        }
    }
    */

    return d_ptr;
}

std::tr1::shared_ptr<std::deque<SolidPtr> > SolidFactory::create_rigid_sphere_matrix(
    ci::Vec3f position, ci::Vec3f radius, int w, int h, int d) {

    std::vector<std::vector<std::vector<btRigidBody*> > > r_bodies;
    std::vector<std::vector<std::vector<ci::Vec3f> > > positions;

    r_bodies.resize(w);
    positions.resize(w);
    for (int i = 0; i < w; ++i) {
        r_bodies[i].resize(h);
        positions[i].resize(h);
        for (int j = 0; j < h; ++j) {
            r_bodies[i][j].resize(d);
            positions[i][j].resize(d);
        }
    }

    ci::Vec3f ptemp = position;
    float r = radius.x;
    float gap = r * 0.3f;
    ci::Vec3f xgap = ci::Vec3f(gap, 0.0f, 0.0f);
    ci::Vec3f ygap = ci::Vec3f(0.0f, gap, 0.0f);
    ci::Vec3f zgap = ci::Vec3f(0.0f, 0.0f, gap);
    ci::Vec3f xdiam = ci::Vec3f(r*2.0f, 0.0f, 0.0f);
    ci::Vec3f ydiam = ci::Vec3f(0.0f, r*2.0f, 0.0f);
    ci::Vec3f zdiam = ci::Vec3f(0.0f, 0.0f, r*2.0f);
    int resolution = 20;

    std::tr1::shared_ptr<std::deque<SolidPtr> > d_ptr = 
        std::tr1::shared_ptr<std::deque<SolidPtr> >(new std::deque<SolidPtr>());

    for (int i = 0; i < w; ++i) {
        for (int j = 0; j < h; ++j) {
            for (int k = 0; k < d; ++k) {
                ci::Vec3f p = position + xgap * i + ygap * j + zgap * k +
                    xdiam * i + ydiam * j + zdiam * k;
                positions[i][j][k] = p;
                r_bodies[i][j][k] = create_bullet_rigid_sphere(p, r);

                d_ptr->push_back(SolidPtr(new RigidSolid(
                    new SphereGraphicItem(radius.x), r_bodies[i][j][k], 
                    SolidFactory::instance().dynamics_world())));
            }
        }
    }

    return d_ptr;
}

std::tr1::shared_ptr<std::deque<SolidPtr> > SolidFactory::create_rigid_sphere_spring_matrix(
    ci::Vec3f position, ci::Vec3f radius, int w, int h, int d) {

    std::vector<std::vector<std::vector<btRigidBody*> > > r_bodies;
    std::vector<std::vector<std::vector<ci::Vec3f> > > positions;

    r_bodies.resize(w);
    positions.resize(w);
    for (int i = 0; i < w; ++i) {
        r_bodies[i].resize(h);
        positions[i].resize(h);
        for (int j = 0; j < h; ++j) {
            r_bodies[i][j].resize(d);
            positions[i][j].resize(d);
        }
    }

    ci::Vec3f ptemp = position;
    float r = radius.x;
    float gap = r * 0.3f;
    ci::Vec3f xgap = ci::Vec3f(gap, 0.0f, 0.0f);
    ci::Vec3f ygap = ci::Vec3f(0.0f, gap, 0.0f);
    ci::Vec3f zgap = ci::Vec3f(0.0f, 0.0f, gap);
    ci::Vec3f xdiam = ci::Vec3f(r*2.0f, 0.0f, 0.0f);
    ci::Vec3f ydiam = ci::Vec3f(0.0f, r*2.0f, 0.0f);
    ci::Vec3f zdiam = ci::Vec3f(0.0f, 0.0f, r*2.0f);
    int resolution = 20;

    std::tr1::shared_ptr<std::deque<SolidPtr> > d_ptr = 
        std::tr1::shared_ptr<std::deque<SolidPtr> >(new std::deque<SolidPtr>());

    for (int i = 0; i < w; ++i) {
        for (int j = 0; j < h; ++j) {
            for (int k = 0; k < d; ++k) {
                ci::Vec3f p = position + xgap * i + ygap * j + zgap * k +
                    xdiam * i + ydiam * j + zdiam * k;
                positions[i][j][k] = p;
                r_bodies[i][j][k] = create_bullet_rigid_sphere(p, r);

                d_ptr->push_back(SolidPtr(new RigidSolid(
                    new SphereGraphicItem(radius.x), r_bodies[i][j][k], 
                    SolidFactory::instance().dynamics_world())));
            }
        }
    }
        
    for (int i = 0; i < w; ++i) {
        for (int j = 0; j < h; ++j) {
            for (int k = 0; k < d; ++k) {
                if (i > 0) {
                    spring_link_rigid_spheres(
                        r_bodies[i-1][j][k],
                        r_bodies[i][j][k],
                        positions[i-1][j][k],
                        positions[i][j][k]);
                }

                if (j > 0) {
                    spring_link_rigid_spheres(
                        r_bodies[i][j-1][k],
                        r_bodies[i][j][k],
                        positions[i][j-1][k],
                        positions[i][j][k]);
                }

                if (k > 0) {
                    spring_link_rigid_spheres(
                        r_bodies[i][j][k-1],
                        r_bodies[i][j][k],
                        positions[i][j][k-1],
                        positions[i][j][k]);
                }
            }
        }
    }
        
    return d_ptr;
}

void SolidFactory::socket_link_soft_spheres(btSoftBody* s1, btSoftBody* s2,
    const ci::Vec3f& p1, const ci::Vec3f& p2) {
    btSoftBody::LJoint::Specs lj;
    lj.cfm = 1;
	lj.erp = 1;
	lj.position = ci::bullet::toBulletVector3((p1 + p2) / 2.0f);
    s1->appendLinearJoint(lj, s2);
}

void SolidFactory::spring_link_rigid_spheres(btRigidBody* r1, btRigidBody* r2,
    const ci::Vec3f& p1, const ci::Vec3f& p2) {

    ci::Vec3f dist = p2 - p1;

	btTransform frameInA, frameInB;
	frameInA = btTransform::getIdentity();
	frameInA.setOrigin(ci::bullet::toBulletVector3(dist));
	frameInB = btTransform::getIdentity();
    frameInB.setOrigin(ci::bullet::toBulletVector3(ci::Vec3f::zero()));

	btGeneric6DofSpringConstraint* spring = 
        new btGeneric6DofSpringConstraint(*r1, *r2, frameInA, frameInB, true);

	spring->setLinearUpperLimit(ci::bullet::toBulletVector3(dist / 2.0f));
	spring->setLinearLowerLimit(ci::bullet::toBulletVector3(dist / -2.0f));

	spring->setAngularLowerLimit(btVector3(0.f, 0.f, -1.5f));
	spring->setAngularUpperLimit(btVector3(0.f, 0.f, 1.5f));

    SolidFactory::instance().dynamics_world()->addConstraint(spring, true);
	spring->setDbgDrawSize(btScalar(5.f));
		
    for (int i = 0; i < 6; ++i) {
		spring->enableSpring(i, true);
		spring->setStiffness(0, 20.f);
        //spring->setStiffness(i, 0.01f);
		spring->setDamping(i, 0.1f); // 0 - 1, 1 == no damping
    }

    spring->setEquilibriumPoint();
}

// N.B. see note in BasicDemo.cpp line 143 on how to increase performance of these
// spheres by reusing a collision shape
btRigidBody* SolidFactory::create_bullet_rigid_sphere(ci::Vec3f position, float radius) {
        
    ci::Quatf rotation = ci::Quatf::identity();

    btCollisionShape* sphere = new btSphereShape((btScalar) radius);
	btDefaultMotionState* motion_state = new btDefaultMotionState(
        btTransform(ci::bullet::toBulletQuaternion(rotation),
        ci::bullet::toBulletVector3(position)));
	
	btVector3 inertia(0,0,0);
	float mass = radius * radius * radius * PI * 4.0f/3.0f;
	sphere->calculateLocalInertia(mass, inertia);
	btRigidBody::btRigidBodyConstructionInfo rigid_body_ci(mass, motion_state, 
        sphere, inertia);
	btRigidBody* rigid_body = new btRigidBody(rigid_body_ci);

    SolidFactory::instance().dynamics_world()->addRigidBody(rigid_body);
        
    rigid_body->setGravity(ci::bullet::toBulletVector3(
        ci::Vec3f(0.0f, SolidFactory::instance().gravity(), 0.0f)));

    return rigid_body;
}

// creates a soft sphere that tries to maintain a constant volume
btSoftBody* SolidFactory::create_bullet_soft_sphere(ci::Vec3f position, 
    ci::Vec3f radius, float res) {
    btVector3 pos = ci::bullet::toBulletVector3(position);
    btVector3 r = ci::bullet::toBulletVector3(radius);

    btSoftBody* soft_body = btSoftBodyHelpers::CreateEllipsoid(
        SolidFactory::instance().soft_body_world_info(),
        pos, r, res);

    /*
    // volume based simulation
	soft_body->m_cfg.kVC = 20;
    soft_body->m_materials[0]->m_kLST = 0.45;
    soft_body->setTotalMass(50, true);
    soft_body->setPose(true,false);
    */

    // pressure based simulation
    soft_body->m_materials[0]->m_kLST = sphere_kLST_;
    soft_body->m_materials[0]->m_kVST = sphere_kVST_;
	soft_body->m_cfg.kDF = sphere_kDF_;
	soft_body->m_cfg.kDP = sphere_kDP_; // fun factor...
	soft_body->m_cfg.kPR = sphere_kPR_;

    soft_body->setTotalMass(sphere_total_mass_);

    soft_body->generateClusters(20);

    // change these for different collision types (with other soft, with ridgid, with static...)
    soft_body->m_cfg.collisions |= btSoftBody::fCollision::VF_SS;
    //soft_body->m_cfg.collisions = btSoftBody::fCollision::CL_SS + 
    //    btSoftBody::fCollision::CL_RS;
    //soft_body->m_cfg.collisions |= btSoftBody::fCollision::SDF_RS;
    //soft_body->m_cfg.collisions |= btSoftBody::fCollision::RVSmask;
    //soft_body->randomizeConstraints();

    SolidFactory::instance().soft_dynamics_world()->addSoftBody(soft_body);

    return soft_body;
}

btDynamicsWorld* SolidFactory::dynamics_world() {
    return dynamics_world_;
}

btSoftRigidDynamicsWorld* SolidFactory::soft_dynamics_world() {
    return dynamics_world_;
}

btSoftBodyWorldInfo& SolidFactory::soft_body_world_info() {
    return soft_body_world_info_;
}

SolidFactory* SolidFactory::instance_;

SolidFactory& SolidFactory::instance() {
    return *instance_;
}

SolidFactory* SolidFactory::instance_ptr() {
    return instance_;
}

void SolidFactory::update_object_gravity() {
    SolidList& solid_list = Manager::instance().solids();
    for (SolidList::const_iterator it =  solid_list.begin();
        it != solid_list.end(); ++it) {
        (*it)->set_gravity(gravity_);
    }
}

float SolidFactory::gravity() {
    return gravity_;
}

bool SolidFactory::set_gravity(float grav) {
    gravity_ = grav;

    return false;
}

float* SolidFactory::gravity_ptr() {
    return &gravity_;
}

// yes, this method does actually work, it's needed for OBJ loading
std::tr1::shared_ptr<ci::TriMesh> SolidFactory::remove_mesh_duplicates(
    const ci::TriMesh& mesh) {

    std::vector<ci::Vec3f> vertices = mesh.getVertices();
	std::vector<size_t> indices = mesh.getIndices();

    std::tr1::shared_ptr<ci::TriMesh> mesh_ptr = 
        std::tr1::shared_ptr<ci::TriMesh>(new ci::TriMesh());

    std::vector<ci::Vec3f> vertex_reduce;
    std::vector<ci::Vec3f>::const_iterator reduce_it;

    float thresh = 0.0000001;

    // NOTE: the threshhold code so far seems really buggy. The code as it stands now
    // removes vertices, but only if they are exact float matches (a relative rarity).
    // the "thresh" code checks distances, but removes a lot of vertices it shouldn't
    bool use_thresh = false;

    std::map<ci::Vec3f, int> remove_index_map;

    for (std::vector<ci::Vec3f>::const_iterator it = vertices.begin();
        it != vertices.end(); ++it) {
        if (use_thresh) {
            bool found = false;
            int i = 0;
            for (reduce_it = vertex_reduce.begin(); reduce_it != vertex_reduce.end();
                ++reduce_it) {
                if (it->distance(*reduce_it) < thresh) {
                    found = true;

                    remove_index_map[*it] = i;

                    break;
                }
                ++i;
            }

            if (found)
                continue;
        } else {
            reduce_it = find(vertex_reduce.begin(), vertex_reduce.end(), *it);
            if (reduce_it != vertex_reduce.end())
                continue;
        }

        vertex_reduce.push_back(*it);
        mesh_ptr->appendVertex(*it);
    }

    // algorithm to find the indice of the vector
    auto find_indice = [&] (const std::vector<ci::Vec3f>& vec, 
        ci::Vec3f val)->int {
        
        // first try looking if it was removed
        if (use_thresh && remove_index_map.find(val) != remove_index_map.end())
            return remove_index_map[val];

        // otherwise we should have an exact match
        for (int i = 0; i < vec.size(); ++i) {
            if (vec[i] == val)
                return i;
        }

        // oh no, this is not good...
        return 0;
    };

    for (std::vector<size_t>::const_iterator it = indices.begin();
        it != indices.end(); ) {

        int in1, in2, in3;

        in1 = find_indice(vertex_reduce, vertices[*it]); ++it;
        in2 = find_indice(vertex_reduce, vertices[*it]); ++it;
        in3 = find_indice(vertex_reduce, vertices[*it]); ++it;

        mesh_ptr->appendTriangle(in1, in2, in3);
    }

    return mesh_ptr;
}

std::tr1::shared_ptr<std::vector<int> > SolidFactory::get_top_vertices(
    const ci::TriMesh& mesh) {

    std::vector<ci::Vec3f> vertices = mesh.getVertices();

    if (vertices.empty())
        return std::tr1::shared_ptr<std::vector<int> >(new std::vector<int>());

    float top_height = vertices[0].y;
    float bottom_height = vertices[0].y;

    for (std::vector<ci::Vec3f>::const_iterator it = vertices.begin();
        it != vertices.end(); ++it) {
        if (it->y >= top_height)
            top_height = it->y;

        if (it->y <= bottom_height)
            bottom_height = it->y;
    }

    float spread = (top_height - bottom_height) / 10.0f;

    std::tr1::shared_ptr<std::vector<int> > indices = 
        std::tr1::shared_ptr<std::vector<int> >(new std::vector<int>());

    float ref_height;

    if (MeshCreator::instance().is_pointed_up()) 
        ref_height = bottom_height;
    else
        ref_height = top_height;

    for (int i = 0; i < vertices.size(); ++i) {
        if (vertices[i].y < (ref_height + spread) &&
            vertices[i].y > (ref_height - spread)) {
            indices->push_back(i);
        }
    }

    return indices;
}

bool SolidFactory::adjust_kDF(float) {
    return physics_param_changed();
}

bool SolidFactory::adjust_kDP(float) {
    return physics_param_changed();
}

bool SolidFactory::adjust_kDG(float) {
    return physics_param_changed();
}

bool SolidFactory::adjust_kPR(float) {
    return physics_param_changed();
}

bool SolidFactory::adjust_kMT(float) {
    return physics_param_changed();
}

bool SolidFactory::physics_param_changed() {
    MeshCreator::instance().rebuild_mesh();

    return false;
}



///////////////////////////////
// DebugDraw

DebugDraw::DebugDraw() : mode_(DBG_NoDebug) {
}

void DebugDraw::drawLine(const btVector3& from, const btVector3& to, 
    const btVector3& color) {
    Color::set_color_a(color.x(), color.y(), color.z(), 0.9f);
    glVertex3f(from.x(), from.y(), from.z());
    glVertex3f(to.x(), to.y(), to.z());
}

void DebugDraw::drawContactPoint(const btVector3& PointOnB, 
    const btVector3& normalOnB, 
    btScalar distance, int lifeTime, const btVector3& color) {
    // Nothing here
}

void DebugDraw::reportErrorWarning(const char* text) {
    ci::app::console() << text << std::endl;
}

void DebugDraw::draw3dText(const btVector3& location, const char* text) {
    // Nothing here
}

void DebugDraw::setDebugMode(int mode) {
    mode_ = mode;
}

int DebugDraw::getDebugMode() const { 
    return mode_;
}

}