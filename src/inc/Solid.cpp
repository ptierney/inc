
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

#include <LinearMath/btIDebugDraw.h>
#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>

// Required for CinderBullet.h
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

#include <inc/Solid.h>
#include <inc/GraphicItem.h>
#include <inc/Manager.h>
#include <inc/BunnyMesh.h>

namespace inc {
    Solid::Solid(GraphicItem* item, btCollisionObject* body, btDynamicsWorld* world) 
        : graphic_item_(item), body_(body), world_(world) {
    }

    Solid::~Solid() {
        delete graphic_item_;

        delete body_;
    }

    void Solid::draw() {
        return;

        graphic_item_->draw();
    }

    btCollisionObject& Solid::collision_object() {
        return *body_;
    }

    bool Solid::detect_selection(ci::app::MouseEvent event, float* distance) {
        return false;
    }


    RigidSolid::RigidSolid(GraphicItem* item, btRigidBody* body, btDynamicsWorld* world) 
        : Solid(item, body, world) {
    }

    RigidSolid::~RigidSolid() {
        if (rigid_body().getMotionState())
            delete rigid_body().getMotionState();
        world_->removeRigidBody(rigid_body_ptr());
    }

    void RigidSolid::draw() {
        ci::Matrix44f tf = ci::bullet::getWorldTransform(rigid_body_ptr());

        glPushMatrix();
            glMultMatrixf(tf.m);
            Solid::draw();
        glPopMatrix();
    }

    void RigidSolid::set_gravity(float g) {
        rigid_body().setGravity(btVector3(0.0, g, 0.0));
        body_->activate();
    }

    // TODO: fix this
    btRigidBody& RigidSolid::rigid_body() {
        return *((btRigidBody*)body_);
    }

    btRigidBody* RigidSolid::rigid_body_ptr() {
        return (btRigidBody*)body_;
    }



    SoftSolid::SoftSolid(GraphicItem* item, btSoftBody* body, btDynamicsWorld* world) 
        : Solid(item, body, world) {
    }

    SoftSolid::~SoftSolid() {
        world_->removeCollisionObject(body_);
    }

    void SoftSolid::draw() {
        Solid::draw();
    }

    // TODO: actually set gravity
    void SoftSolid::set_gravity(float g) {
        body_->activate();
    }

    // TODO: fix this
    btSoftBody& SoftSolid::soft_body() {
        return *((btSoftBody*)body_);
    }

    btSoftBody* SoftSolid::soft_body_ptr() {
        return (btSoftBody*)body_;
    }



    SolidFactory::SolidFactory() {
        instance_ = this;
        gravity_ = -9.8f;//0.0f;//-9.8f;
        last_gravity_ = gravity_;
    }

    void SolidFactory::setup() {
        init_physics();

        interface_ = ci::params::InterfaceGl("Solids_Factory", ci::Vec2i(100, 200));
        interface_.addParam("Gravity", &gravity_, 
            "min=-20.0 max=20.0 step=0.1 keyIncr=g keyDecr=G");
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
            btIDebugDraw::DBG_DrawWireframe);
            //btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawAabb);
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
        glLineWidth(0.9f);
        glBegin(GL_LINES);
        dynamics_world_->debugDrawWorld();
        glEnd();
    }

    SolidFactory::~SolidFactory() {
        delete debug_draw_;

        delete dynamics_world_;

        delete solver_;

        delete broadphase_;

        delete dispatcher_;

        delete collision_configuration_;
    }

    SolidPtr SolidFactory::create_solid_box(ci::Vec3f dimensions, 
        ci::Vec3f position) {
        btRigidBody* body = ci::bullet::createBox(SolidFactory::instance().dynamics_world(), 
            dimensions, ci::Quatf(), position);

        GraphicItem* item = new BoxGraphicItem(dimensions);

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
        GraphicItem* item = new VboGraphicItem(vbo_mesh, scale);

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

        GraphicItem* item = new PlaneGraphicItem(dimension);

        SolidPtr solid(new RigidSolid(item, body, 
            SolidFactory::instance().dynamics_world()));

        return solid;
    }

    SolidPtr SolidFactory::create_static_solid_box(ci::Vec3f dimensions,
        ci::Vec3f position) {
        btCollisionShape* box = new btBoxShape(
            ci::bullet::toBulletVector3(dimensions) / 2.0f);

		btDefaultMotionState* motionState = 
            new btDefaultMotionState(
            btTransform(ci::bullet::toBulletQuaternion(ci::Quatf()),
            ci::bullet::toBulletVector3(position)));
		
		btVector3 inertia(0,0,0);
		float mass = 0.0f; // objects of mass 0 do not move
		box->calculateLocalInertia(mass, inertia);
		btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(mass, motionState, 
            box, inertia);
		
		btRigidBody *rigid_body = new btRigidBody(rigidBodyCI);
		SolidFactory::instance().dynamics_world()->addRigidBody(rigid_body);

        GraphicItem* item = new BoxGraphicItem(dimensions);

        SolidPtr solid(new RigidSolid(item, rigid_body, 
            SolidFactory::instance().dynamics_world()));

        return solid;
    }

    SolidPtr SolidFactory::create_soft_sphere(ci::Vec3f position, ci::Vec3f radius) {
        btSoftBody*	soft_body = create_bullet_soft_sphere(position, radius, 20);

        SolidPtr solid(new SoftSolid(NULL, soft_body, 
            SolidFactory::instance().dynamics_world()));

        return solid;
    }

    std::tr1::shared_ptr<std::deque<SolidPtr> > SolidFactory::create_linked_soft_spheres(
        ci::Vec3f position, ci::Vec3f radius) {

        ci::Vec3f offset = ci::Vec3f(0.0f, 5.0f, 0.0f);

        ci::Vec3f p1 = position + offset;
        ci::Vec3f p2 = position - offset;

        btSoftBody* sb1 = create_bullet_soft_sphere(
            p1, radius, 20);
        btSoftBody* sb2 = create_bullet_soft_sphere(
            p2, radius, 20);

        socket_link_soft_spheres(sb1, sb2, p1, p2);

        std::tr1::shared_ptr<std::deque<SolidPtr> > d_ptr = 
            std::tr1::shared_ptr<std::deque<SolidPtr> >(new std::deque<SolidPtr>());

        d_ptr->push_back(SolidPtr(new SoftSolid(NULL, sb1, 
            SolidFactory::instance().dynamics_world())));
        d_ptr->push_back(SolidPtr(new SoftSolid(NULL, sb2, 
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
        float gap = r/2.0f;
        ci::Vec3f xgap = ci::Vec3f(gap, 0.0f, 0.0f);
        ci::Vec3f ygap = ci::Vec3f(0.0f, gap, 0.0f);
        ci::Vec3f zgap = ci::Vec3f(0.0f, 0.0f, gap);
        ci::Vec3f xdiam = ci::Vec3f(r*2.0f, 0.0f, 0.0f);
        ci::Vec3f ydiam = ci::Vec3f(0.0f, r*2.0f, 0.0f);
        ci::Vec3f zdiam = ci::Vec3f(0.0f, 0.0f, r*2.0f);
        int resolution = 30;

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

                    d_ptr->push_back(SolidPtr(new SoftSolid(NULL, s_bodies[i][j][k], 
                        SolidFactory::instance().dynamics_world())));
                }
            }
        }

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

    // creates a soft sphere that tries to maintain a constant volume
    btSoftBody* SolidFactory::create_bullet_soft_sphere(ci::Vec3f position, 
        ci::Vec3f radius, float res) {
        btVector3 pos = ci::bullet::toBulletVector3(position);
        btVector3 r = ci::bullet::toBulletVector3(radius);

        btSoftBody* soft_body = btSoftBodyHelpers::CreateEllipsoid(
            SolidFactory::instance().soft_body_world_info(),
            pos, r, res);

        btSoftBody::Material* pm = soft_body->appendMaterial();
	    pm->m_kLST = 0.45;
	    pm->m_flags -= btSoftBody::fMaterial::DebugDraw;			
	    soft_body->generateBendingConstraints(2, pm);

        soft_body->m_cfg.kVC = 20;
        soft_body->m_cfg.kDF = 1;
        soft_body->setTotalMass(50, true);
        soft_body->setPose(true,false);
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

    SolidPtr SolidFactory::create_sphere_container() {
        ci::ObjLoader loader(ci::loadFileStream("/projects/inc/sock4.obj"));
        ci::TriMesh mesh;
        loader.load(&mesh, true);

        std::vector<ci::Vec3f> vertices = mesh.getVertices();
		std::vector<size_t> indices = mesh.getIndices();
		
		btTriangleMesh* tmesh = new btTriangleMesh(true, false);
		
        for(int i = 0; i < mesh.getNumIndices(); i += 3) {
			tmesh->addTriangle(ci::bullet::toBulletVector3(vertices[indices[i]]), 
			    ci::bullet::toBulletVector3(vertices[indices[i+1]]), 
				ci::bullet::toBulletVector3(vertices[indices[i+2]]), 
				true);
		}

		btBvhTriangleMeshShape* tri_mesh = new btBvhTriangleMeshShape(tmesh, true, true);
        tri_mesh->setLocalScaling(ci::bullet::toBulletVector3(ci::Vec3f::one() * 10.0f));
		tri_mesh->setMargin(0.05f);

        btDefaultMotionState *motion_state = new btDefaultMotionState(btTransform(
            ci::bullet::toBulletQuaternion(ci::Quatf(-M_PI / 2.0f, 0.0f, 0.0f)),
            ci::bullet::toBulletVector3((ci::Vec3f(0.0, 5.0f, 0.0f)))));
		btRigidBody::btRigidBodyConstructionInfo body_ci(0.0f, motion_state, tri_mesh, btVector3(0,0,0));
		btRigidBody* rigid_body = new btRigidBody(body_ci);

        SolidFactory::instance().soft_dynamics_world()->addRigidBody(rigid_body);

        SolidPtr solid(new RigidSolid(NULL, rigid_body,
            SolidFactory::instance().dynamics_world()));

        return solid;
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

    void SolidFactory::update_object_gravity() {
        SolidList& solid_list = Manager::instance().solids();
        for (SolidList::const_iterator it =  solid_list.begin();
            it != solid_list.end(); ++it) {
            (*it)->set_gravity(gravity_);
        }
    }


    DebugDraw::DebugDraw() : mode_(DBG_NoDebug) {
    }

    void DebugDraw::drawLine(const btVector3& from, const btVector3& to, 
        const btVector3& color) {
        glColor4f(color.x(), color.y(), color.z(), 0.9f);
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