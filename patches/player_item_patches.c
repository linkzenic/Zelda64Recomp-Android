#include "patches.h"

#if !defined(ZELDA_ANDROID_BUILTIN_DPAD)
RECOMP_PATCH ItemId Player_GetItemOnButton(PlayState* play, Player* player, EquipSlot slot) {
    if (slot >= EQUIP_SLOT_A) {
        return ITEM_NONE;
    }

    if (slot == EQUIP_SLOT_B) {
        ItemId item = Inventory_GetBtnBItem(play);

        if (item >= ITEM_FD) {
            return item;
        }

        if ((player->currentMask == PLAYER_MASK_BLAST) && (play->interfaceCtx.bButtonDoAction == DO_ACTION_EXPLODE)) {
            return ITEM_F0;
        }

        if ((player->currentMask == PLAYER_MASK_BREMEN) && (play->interfaceCtx.bButtonDoAction == DO_ACTION_MARCH)) {
            return ITEM_F1;
        }

        if ((player->currentMask == PLAYER_MASK_KAMARO) && (play->interfaceCtx.bButtonDoAction == DO_ACTION_DANCE)) {
            return ITEM_F2;
        }

        return item;
    }

    if (slot == EQUIP_SLOT_C_LEFT) {
        return C_BTN_ITEM(EQUIP_SLOT_C_LEFT);
    }

    if (slot == EQUIP_SLOT_C_DOWN) {
        return C_BTN_ITEM(EQUIP_SLOT_C_DOWN);
    }

    return C_BTN_ITEM(EQUIP_SLOT_C_RIGHT);
}
#endif

// Player_GetItemOnButton is patched in dpad_builtin.c so Android can keep the
// built-in D-Pad feature without runtime code-page mutation.
