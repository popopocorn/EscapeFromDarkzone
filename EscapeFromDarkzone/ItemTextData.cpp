#include "stdafx.h"
#include "ItemTextData.h"
#include "ItemDef.h"

const wchar_t* GetItemDisplayName(ItemID itemId)
{
	switch (itemId)
	{
	case ItemID::MAT_1_FIBER:
		return L"섬유";

	case ItemID::MAT_2_METAL_PLATE:
		return L"금속판";

	case ItemID::MAT_3_BOLT_AND_NUT:
		return L"볼트와 너트";

	/*case ItemID::WEAPON_UPGRADE_1:
		return L"무기 강화 부품 1";*/

	case ItemID::WEAPON_UPGRADE_2:
		return L"무기 강화 부품 2";

	case ItemID::WEAPON_UPGRADE_3:
		return L"무기 강화 부품 3";

	case ItemID::WEAPON_UPGRADE_4:
		return L"무기 강화 부품 4";

	case ItemID::ARMOR_PLATE:
		return L"방어구 강화판";

	case ItemID::WEAPON_RIFLE_01:
		return L"1단계 소총";

	case ItemID::WEAPON_RIFLE_02:
		return L"2단계 소총";

	case ItemID::WEAPON_RIFLE_03:
		return L"3단계 소총";

	case ItemID::WEAPON_RIFLE_04:
		return L"4단계 소총";

	case ItemID::WEAPON_SMG_01:
		return L"1단계 기관단총";

	case ItemID::WEAPON_SMG_02:
		return L"2단계 기관단총";

	case ItemID::WEAPON_SMG_03:
		return L"3단계 기관단총";

	case ItemID::WEAPON_SMG_04:
		return L"4단계 기관단총";

	case ItemID::WEAPON_SHOTGUN_01:
		return L"1단계 산탄총";

	case ItemID::WEAPON_SHOTGUN_02:
		return L"2단계 산탄총";

	case ItemID::WEAPON_SHOTGUN_03:
		return L"3단계 산탄총";

	case ItemID::WEAPON_SHOTGUN_04:
		return L"4단계 산탄총";

	case ItemID::ARMOR_HELMET_01:
		return L"1단계 헬멧";

	case ItemID::ARMOR_HELMET_02:
		return L"2단계 헬멧";

	case ItemID::ARMOR_HELMET_03:
		return L"3단계 헬멧";

	case ItemID::ARMOR_HELMET_04:
		return L"4단계 헬멧";

	case ItemID::ARMOR_BODY_01:
		return L"1단계 방탄복";

	case ItemID::ARMOR_BODY_02:
		return L"2단계 방탄복";

	case ItemID::ARMOR_BODY_03:
		return L"3단계 방탄복";

	case ItemID::ARMOR_BODY_04:
		return L"4단계 방탄복";

	case ItemID::ARMOR_SHOES_01:
		return L"1단계 전투화";

	case ItemID::ARMOR_SHOES_02:
		return L"2단계 전투화";

	case ItemID::ARMOR_SHOES_03:
		return L"3단계 전투화";

	case ItemID::ARMOR_SHOES_04:
		return L"4단계 전투화";

	case ItemID::ESCAPE_KEY:
		return L"탈출 열쇠";

	case ItemID::NONE:
		return L"";

	default:
		return L"알 수 없는 아이템";
	}
}