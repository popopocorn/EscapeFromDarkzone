#pragma once

enum class ItemID : short {
	//犁丰
	NONE = 0,
	MAT_1_FIBER,
	MAT_2_METAL_PLATE,
	MAT_3_BOLT_AND_NUT,
	WEAPON_UPGRADE_1,
	WEAPON_UPGRADE_2,
	WEAPON_UPGRADE_3,
	WEAPON_UPGRADE_4,
	ARMOR_PLATE,
	//肯己前
	WEAPON_RIFLE,
	ARMOR_VEST,
};

constexpr int MAX_SLOTS = 10;

struct ItemSlot {
	ItemID item = ItemID::NONE;
	int    count = 0;
};

struct RecipeIngredient {
	ItemID item;
	int    count;
};

constexpr int MAX_RECIPE_INGREDIENTS = 4;
struct CraftRecipe {
	ItemID           result;
	int              resultCount;
	RecipeIngredient ingredients[MAX_RECIPE_INGREDIENTS];
};

inline constexpr CraftRecipe g_craftRecipes[] = {
	// 公扁: MAT_2 x20 + MAT_3 x20
	{ ItemID::WEAPON_RIFLE, 1, {
		{ ItemID::MAT_2_METAL_PLATE, 20 },
		{ ItemID::MAT_3_BOLT_AND_NUT, 20 },
		{ ItemID::NONE, 0 },
		{ ItemID::NONE, 0 } } },
	// 规绢备: MAT_1 x10 + MAT_2 x10 + MAT_3 x10
	{ ItemID::ARMOR_VEST,   1, {
		{ ItemID::MAT_1_FIBER, 10 },
		{ ItemID::MAT_2_METAL_PLATE, 10 },
		{ ItemID::MAT_3_BOLT_AND_NUT, 10 },
		{ ItemID::NONE, 0 } } },
};

constexpr int g_craftRecipeCount = static_cast<int>(sizeof(g_craftRecipes) / sizeof(g_craftRecipes[0]));

inline const CraftRecipe* FindCraftRecipe(ItemID target) {
	for (int i = 0; i < g_craftRecipeCount; ++i) {
		if (g_craftRecipes[i].result == target) return &g_craftRecipes[i];
	}
	return nullptr;
}