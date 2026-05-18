#pragma once

enum class ItemID : short {
    NONE = 0,
    MAT_1, MAT_2, MAT_3, MAT_4, MAT_5, MAT_6,
    WEAPON_UPGRADE_1, WEAPON_UPGRADE_2, WEAPON_UPGRADE_3, WEAPON_UPGRADE_4,
    ARMOR_PLATE,
};

struct ItemSlot {
    ItemID item = ItemID::NONE;
    int    count = 0;
};
