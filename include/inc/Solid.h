
/*  Copyright (C) 2010 Patrick Tierney
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

#include <deque>

#include <btBulletDynamicsCommon.h>
#include <LinearMath/btIDebugDraw.h>
#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>

#include <cinder/app/MouseEvent.h>

#include <inc/GraphicItem.h>
#include <inc/Module.h>

namespace cinder {
    class TriMesh;
}

namespace inc {
    class Solid {
    public:
        Solid(GraphicItem*, btCollisionObject*, btDynamicsWorld*);
        virtual ~Solid();

        virtual void draw();
        virtual void set_gravity(float) = 0;
        virtual btCollisionObject& collision_object();
        virtual bool detect_selection(ci::app::MouseEvent, float* dist);

    protected:
        GraphicItem* graphic_item_;
        btCollisionObject* body_;
        btDynamicsWorld* world_;

    };

    class RigidSolid : public Solid {
    public:
        RigidSolid(GraphicItem*, btRigidBody*, btDynamicsWorld*);
        virtual ~RigidSolid();

        virtual void draw();
        virtual void set_gravity(float);
        virtual btRigidBody& rigid_body();
        virtual btRigidBody* rigid_body_ptr();
    };

    class SoftSolid : public Solid {
    public:
        SoftSolid(GraphicItem*, btSoftBody*, btDynamicsWorld*);
        virtual ~SoftSolid();

        virtual void draw();
        virtual void set_gravity(float);
        virtual btSoftBody& soft_body();
        virtual btSoftBody* soft_body_ptr();
    };

    class DebugDraw;

    typedef std::tr1::shared_ptr<Solid> SolidPtr;

    class SolidFactory : public Module {
    public:
        SolidFactory();
        ~SolidFactory();

        void setup();
        void update();
        void draw();

        float time_step();
        btDynamicsWorld* dynamics_world();
        btSoftRigidDynamicsWorld* soft_dynamics_world();
        float gravity();


        static SolidPtr create_soft_sphere(ci::Vec3f position, ci::Vec3f radius);
        static std::tr1::shared_ptr<std::deque<SolidPtr> > create_linked_soft_spheres(
            ci::Vec3f pos, ci::Vec3f radius);
        static std::tr1::shared_ptr<std::deque<SolidPtr> > create_soft_sphere_matrix(
            ci::Vec3f pos, ci::Vec3f radius, int w, int h, int d);
        static std::tr1::shared_ptr<std::deque<SolidPtr> > create_rigid_sphere_matrix(
            ci::Vec3f pos, ci::Vec3f radius, int w, int h, int d);

        static SolidPtr create_sphere_container();

        // I don't use these
        static SolidPtr create_plane(ci::Vec3f dimensions, ci::Vec3f position);
        static SolidPtr create_static_solid_box(ci::Vec3f dimensions, 
            ci::Vec3f position);
        static SolidPtr create_solid_box(ci::Vec3f dimensions, 
            ci::Vec3f position);
        static SolidPtr create_rigid_mesh(ci::TriMesh&, 
            ci::Vec3f position, 
            ci::Vec3f scale, float mass);

        static SolidFactory& instance();

        btSoftBodyWorldInfo& soft_body_world_info();

        void delete_constraints();

    private:
        void init_physics();
        void update_object_gravity();
        
        static btSoftBody* create_bullet_soft_sphere(ci::Vec3f position, 
            ci::Vec3f radius, float res);
        static void socket_link_soft_spheres(btSoftBody* s1, btSoftBody* s2,
            const ci::Vec3f& p1, const ci::Vec3f& p2);
        static void spring_link_rigid_spheres(btRigidBody* r1, btRigidBody* r2,
            const ci::Vec3f& p1, const ci::Vec3f& p2);
        static btRigidBody* create_bullet_rigid_sphere(ci::Vec3f position,
            float radius);

        static std::tr1::shared_ptr<ci::TriMesh> remove_mesh_duplicates(
            const ci::TriMesh& mesh);

        //btDynamicsWorld* dynamics_world_;
        btSoftRigidDynamicsWorld* dynamics_world_;
        btSoftBodyWorldInfo soft_body_world_info_;
        btDefaultCollisionConstructionInfo collision_info_;

        btDefaultCollisionConfiguration* collision_configuration_;
        btCollisionDispatcher* dispatcher_;
        btBroadphaseInterface* broadphase_;
        btSequentialImpulseConstraintSolver* solver_;
        DebugDraw* debug_draw_;

        static std::deque<btTriangleMesh*> mesh_cleanup_;

        ci::params::InterfaceGl interface_;

        double time_step_;
        double last_time_;

        float gravity_;
        float last_gravity_;

        static SolidFactory* instance_;
    };

    /*
	enum	DebugDrawModes
	{
		DBG_NoDebug=0,
		DBG_DrawWireframe = 1,
		DBG_DrawAabb=2,
		DBG_DrawFeaturesText=4,
		DBG_DrawContactPoints=8,
		DBG_NoDeactivation=16,
		DBG_NoHelpText = 32,
		DBG_DrawText=64,
		DBG_ProfileTimings = 128,
		DBG_EnableSatComparison = 256,
		DBG_DisableBulletLCP = 512,
		DBG_EnableCCD = 1024,
		DBG_DrawConstraints = (1 << 11),
		DBG_DrawConstraintLimits = (1 << 12),
		DBG_FastWireframe = (1<<13),
		DBG_MAX_DEBUG_DRAW_MODE
	};
    */

    class DebugDraw : public btIDebugDraw {
    public:
        DebugDraw();
        void drawLine(const btVector3& from, const btVector3& to, const btVector3& color);
        void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, 
            btScalar distance, int lifeTime, const btVector3& color);
        void reportErrorWarning(const char* text);

        void draw3dText(const btVector3& location, const char* text);
        void setDebugMode(int mode);
        int getDebugMode() const; 

    private:
        int mode_;

    };
}