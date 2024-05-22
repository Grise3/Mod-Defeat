#ifndef HIVE_COMPONENT_H_
#define HIVE_COMPONENT_H_

#include "../backend/CBSEBackend.h"
#include "../backend/CBSEComponents.h"

class HiveComponent: public HiveComponentBase {
	public:
		// ///////////////////// //
		// Autogenerated Members //
		// ///////////////////// //

		/**
		 * @brief Default constructor of the HiveComponent.
		 * @param entity The entity that owns the component instance.
		 * @param r_AlienBuildableComponent A AlienBuildableComponent instance that this component depends on.
		 * @note This method is an interface for autogenerated code, do not modify its signature.
		 */
		HiveComponent(Entity& entity, AlienBuildableComponent& r_AlienBuildableComponent);

		/**
		 * @brief Handle the Damage message.
		 * @param amount
		 * @param source
		 * @param location
		 * @param direction
		 * @param flags
		 * @param meansOfDeath
		 * @note This method is an interface for autogenerated code, do not modify its signature.
		 */
		void HandleDamage(float amount, gentity_t* source, Util::optional<glm::vec3> location, Util::optional<glm::vec3> direction, int flags, meansOfDeath_t meansOfDeath);

		// ///////////////////// //

	private:

		void Think(int timeDelta);

		Entity* FindTarget();

		bool TargetValid(Entity& candidate, bool checkDistance) const;

		bool CompareTargets(Entity& a, Entity& b) const;

		void Fire(Entity& target);

		bool insectsReady;
		int  insectsActiveSince;
};

#endif // HIVE_COMPONENT_H_
