#include <objects/ProjectileObject.hpp>
#include <engine/GameWorld.hpp>
#include <data/WeaponData.hpp>

void ProjectileObject::checkPhysicsContact()
{
	btManifoldArray   manifoldArray;
	btBroadphasePairArray& pairArray = _ghostBody->getOverlappingPairCache()->getOverlappingPairArray();
	int numPairs = pairArray.size();

	for (int i=0;i<numPairs;i++) {
		manifoldArray.clear();

		const btBroadphasePair& pair = pairArray[i];

		//unless we manually perform collision detection on this pair, the contacts are in the dynamics world paircache:
		btBroadphasePair* collisionPair = engine->dynamicsWorld->getPairCache()->findPair(pair.m_pProxy0,pair.m_pProxy1);
		if (!collisionPair) {
			continue;
		}

		if (collisionPair->m_algorithm) {
			collisionPair->m_algorithm->getAllContactManifolds(manifoldArray);
		}

		for (int j=0;j<manifoldArray.size();j++) {
			btPersistentManifold* manifold = manifoldArray[j];

			/// @todo check if this is a suitable level to check c.f btManifoldPoint
			// It's happening
			explode();
		}
	}
}

void ProjectileObject::explode()
{
	if( ! _exploded ) {
		// Remove our physics objects
		cleanup();

		/// @todo accelerate this with bullet instead of doing this stupid loop.
		for(auto& o : engine->objects) {
			if( o == this ) continue;
			switch( o->type() ) {
			case GameObject::Instance:
			case GameObject::Vehicle:
			case GameObject::Character:
				break;
			default:
				continue;
			}

			float d = glm::distance(getPosition(), o->getPosition());
			if( d > 10.f ) continue;

			o->takeDamage({
							  getPosition(),
							  getPosition(),
							  10.f / glm::max(d, 1.f),
							  DamageInfo::Explosion,
							  0.f
						  });
			_exploded = true;
		}
	}
}

void ProjectileObject::cleanup()
{
	if( _body ) {
		engine->dynamicsWorld->removeRigidBody(_body);
		delete _body;
		_body = nullptr;
	}
	if( _ghostBody ) {
		engine->dynamicsWorld->removeCollisionObject(_ghostBody);
		delete _ghostBody;
		_ghostBody = nullptr;
	}
	if( _shape ) {
		delete _shape;
		_shape = nullptr;
	}
}

ProjectileObject::ProjectileObject(GameWorld *world, const glm::vec3 &position, const ProjectileObject::ProjectileInfo &info)
	: GameObject(world, position, glm::quat(), nullptr),
	  _info(info), _body(nullptr), _ghostBody(nullptr),
	  _exploded(false)
{
	_shape = new btSphereShape(0.25f);
	btVector3 inertia(0.f, 0.f, 0.f);
	_shape->calculateLocalInertia(1.f, inertia);
	btRigidBody::btRigidBodyConstructionInfo riginfo(1.f, nullptr, _shape, inertia);

	riginfo.m_startWorldTransform = btTransform(btQuaternion(), btVector3(position.x, position.y, position.z));
	riginfo.m_mass = 1.f;

	_body = new btRigidBody(riginfo);
	_body->setUserPointer(this);
	_body->setLinearVelocity(btVector3(_info.direction.x, _info.direction.y, _info.direction.z) * _info.velocity);
	engine->dynamicsWorld->addRigidBody(_body, btBroadphaseProxy::DefaultFilter,
										btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter );

	if( _info.type == RPG ) {
		// RPGs aren't affected by gravity
		_body->setGravity( { 0.f, 0.f, 0.f } );
	}

	if( _info.type != Grenade ) {
		// Projectiles that aren't grenades explode on contact.
		_ghostBody = new btPairCachingGhostObject();
		_ghostBody->setWorldTransform(_body->getWorldTransform());
		_ghostBody->setCollisionShape(_shape);
		_ghostBody->setUserPointer(this);
		_ghostBody->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT|btCollisionObject::CF_NO_CONTACT_RESPONSE);
		engine->dynamicsWorld->addCollisionObject(_ghostBody, btBroadphaseProxy::SensorTrigger);
	}
}

ProjectileObject::~ProjectileObject()
{
	cleanup();
}

void ProjectileObject::tick(float dt)
{
	if( _body == nullptr ) return;

	auto& bttr = _body->getWorldTransform();
	position = { bttr.getOrigin().x(), bttr.getOrigin().y(), bttr.getOrigin().z() };

	_info.time -= dt;

	if( _ghostBody ) {
		_ghostBody->setWorldTransform(_body->getWorldTransform());
		checkPhysicsContact();
	}

	if( _info.time <= 0.f ) {
		explode();
	}
}
