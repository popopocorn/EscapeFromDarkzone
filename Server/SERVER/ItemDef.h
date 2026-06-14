#pragma once

//아이템 선언
enum class ItemType {
	PISTOL,
	RIFLE,
	SMG,
	SHOTGUN,
	ARMOR_HELMET,//
	ARMOR_BODY,
	ARMOR_SHOES,
	PLATE,
	CONSUMABLE,
	MATERIAL,
};

enum class ItemGrade
{
	BASIC,
	GRADE_1,
	GRADE_2,
	GRADE_3,
	GRADE_4
};


enum class ItemID : short {
	//재료
	NONE = 0,
	MAT_1_FIBER,
	MAT_2_METAL_PLATE,
	MAT_3_BOLT_AND_NUT,
	WEAPON_UPGRADE_1,
	WEAPON_UPGRADE_2,
	WEAPON_UPGRADE_3,
	WEAPON_UPGRADE_4,
	ARMOR_PLATE,
	//완성품
	WEAPON_RIFLE,

	ARMOR_HELMET_01,
	ARMOR_HELMET_02,
	ARMOR_HELMET_03,
	ARMOR_HELMET_04,

	ARMOR_BODY_01,
	ARMOR_BODY_02,
	ARMOR_BODY_03,
	ARMOR_BODY_04,

	ARMOR_SHOES_01,
	ARMOR_SHOES_02,
	ARMOR_SHOES_03,
	ARMOR_SHOES_04,


	ITEMID_END
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
	// 무기: MAT_2 x20 + MAT_3 x20
	{ ItemID::WEAPON_RIFLE, 1, {
		{ ItemID::MAT_2_METAL_PLATE, 20 },
		{ ItemID::MAT_3_BOLT_AND_NUT, 20 },
		{ ItemID::NONE, 0 },
		{ ItemID::NONE, 0 } } },
	// 방어구: MAT_1 x10 + MAT_2 x10 + MAT_3 x10
	{ ItemID::ARMOR_BODY_01,   1, {
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