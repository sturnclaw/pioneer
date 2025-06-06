// Copyright © 2008-2025 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _LOADERDEFINITIONS_H_
#define _LOADERDEFINITIONS_H_
/*
 * Data strcutrures used by Loader
 */
#include "Color.h"
#include "../vector3.h"

#include <string>
#include <vector>

namespace SceneGraph {

	struct MaterialDefinition {
		MaterialDefinition(const std::string &n) :
			name(n),
			tex_diff(""),
			tex_spec(""),
			tex_glow(""),
			tex_ambi(""),
			tex_norm(""),
			diffuse(Color::WHITE),
			specular(Color::WHITE),
			ambient(Color::BLANK),
			emissive(Color::BLANK),
			shininess(100),
			opacity(100),
			alpha_test(false),
			unlit(false),
			use_pattern(false)
		{}
		std::string name;
		std::string tex_diff;
		std::string tex_spec;
		std::string tex_glow;
		std::string tex_ambi;
		std::string tex_norm;
		Color diffuse;
		Color specular;
		Color ambient;
		Color emissive;
		unsigned int shininess; //specular power, 0-128
		unsigned int opacity; //0-100
		bool alpha_test;
		bool unlit;
		bool use_pattern;
	};

	struct LodDefinition {
		LodDefinition(float size) :
			pixelSize(size)
		{}
		float pixelSize;
		std::vector<std::string> meshNames;
	};

	struct AnimDefinition {
		AnimDefinition(const std::string &name_, double start_, double end_, bool loop_) :
			name(name_),
			start(start_),
			end(end_),
			loop(loop_)
		{}
		std::string name;
		double start;
		double end;
		bool loop;
	};

	struct BoundDefinition {
		enum Type {
			// Capsule (cylinder with rounded end-caps)
			// startTag, endTag define the endpoints of the central line of the cylinder
			// radius defines both the radius of the cylinder and the radius of the endcaps
			CAPSULE,
		};
		Type type;
		std::string startTag;
		std::string endTag;
		double radius;

		// What boundary does this bound definition refer to?
		std::string forBound;

		static BoundDefinition create_capsule(const std::string &for_b, const std::string &start,
			const std::string &end, const double rad) {
			BoundDefinition out;
			out.type = CAPSULE;
			out.startTag = start;
			out.endTag = end;
			out.radius = rad;
			out.forBound = for_b;
			return out;
		}

	};

	struct ModelDefinition {
		std::string name;
		std::vector<LodDefinition> lodDefs;
		std::vector<MaterialDefinition> matDefs;
		std::vector<std::string> collisionDefs;
		std::vector<AnimDefinition> animDefs;
		std::vector<BoundDefinition> boundsDefs;
	};

} // namespace SceneGraph

#endif
