#pragma once


//재료를 위한 구조
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
	WEAPON_RIFLE,     // 더미 무기
	ARMOR_VEST,       // 더미 방어구
};
struct ItemSlot {
    ItemID item = ItemID::NONE;
    int    count = 0;
};
