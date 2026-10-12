var OfflineInventory = (function () {
    var team = 3, category = 'all', selectedItem = null;
    var inventory = [], skins = [], pendingSkin = null, skinPage = 0;
    var pageSize = 12, saving = false, status = '';
    var stickerSkin = null, stickerCatalog = null, stickerSlots = [], stickerSlot = 0, pendingSticker = 0;
    var categories = [
        ['all', '全部'], ['secondary', '手枪'], ['smg', '冲锋枪'], ['rifle', '步枪'],
        ['sniper', '狙击枪'], ['shotgun', '霰弹枪'], ['machinegun', '机枪'], ['melee', '刀具'], ['gloves', '手套']
    ];
    function Root() { return $.GetContextPanel(); }
    function Find(id) { return Root().FindChildInLayoutFile(id); }
    function Name(value) { return value && value.charAt(0) === '#' ? $.Localize(value) : value; }
    function EngineImage(image, definition, paint) {
        // Only bundled skin artwork is used; stickers are shown on the gun.
        if (image && image.indexOf('file://{images}/') === 0) return image;
        return 'file://{images}/offline_skins/' + definition + '_' + paint + '.png';
    }
    function Eligible(item, value) { return !!item.teams[value]; }
    function Variant(item, value) { return item.teams[value] || item.teams[3] || item.teams[2]; }
    function SharedGun(item) { return item.kind !== 'melee' && item.kind !== 'gloves' && Eligible(item, 3) && Eligible(item, 2); }
    function TeamList() { return category === 'melee' || category === 'gloves'; }
    function SeparateEquip(item) { return item.kind === 'melee' || item.kind === 'gloves'; }
    function FindItem(definition) {
        for (var i = 0; i < inventory.length; ++i) if (inventory[i].definition === definition) return inventory[i];
        return null;
    }
    function InventoryRow(item, value, side) {
        var row = Variant(item, value), ct = item.teams[3], t = item.teams[2];
        return { id: side ? (item.kind === 'gloves' ? 'OfflineGlove' : 'OfflineKnife') + (side === 3 ? 'CT' : 'T') + item.definition : 'OfflineItem' + item.definition,
            definition: item.definition, name: item.name, kind: item.kind, image: row.image, paint: row.paint,
            subtitle: Name(row.skin_name), ct: !!ct, t: !!t, side: side || 0,
            equipped_ct: !!(ct && ct.equipped), equipped_t: !!(t && t.equipped),
            ct_action: 'OfflineEquipCT' + item.definition, t_action: 'OfflineEquipT' + item.definition };
    }
    function InventoryRows() {
        var rows = [];
        if (TeamList()) {
            var ct = [], t = [];
            inventory.forEach(function (item) {
                if (item.kind !== category) return;
                if (Eligible(item, 3)) ct.push(InventoryRow(item, 3, 3));
                if (Eligible(item, 2)) t.push(InventoryRow(item, 2, 2));
            });
            // Keep CT in the left column and T in the right even if a later
            // schema adds a team-only item. Empty cells retain that alignment.
            for (var i = 0; i < Math.max(ct.length, t.length); ++i) {
                rows.push(ct[i] || { placeholder: true, side: 3 });
                rows.push(t[i] || { placeholder: true, side: 2 });
            }
        } else inventory.forEach(function (item) {
            if (category === 'all' || item.kind === category) rows.push(InventoryRow(item, team, 0));
        });
        return rows;
    }
    function Status(value) { status = value; Find('OfflineInventoryStatus').text = value; Publish(); }
    function ReadInventory() {
        var byDefinition = {};
        [3, 2].forEach(function (value) {
            (NativeInventoryAPI.GetInventory(value) || []).forEach(function (row) {
                row.image = EngineImage(row.image,row.definition,row.paint);
                var item = byDefinition[row.definition];
                if (!item) item = byDefinition[row.definition] = {
                    definition: row.definition, kind: row.kind, name: Name(row.name), teams: {}
                };
                item.teams[value] = row;
            });
        });
        inventory = Object.keys(byDefinition).map(function (key) { return byDefinition[key]; });
        var order = ['secondary', 'smg', 'rifle', 'sniper', 'shotgun', 'machinegun', 'melee', 'gloves'];
        inventory.sort(function (a, b) {
            return order.indexOf(a.kind) - order.indexOf(b.kind) || a.definition - b.definition;
        });
    }
    function Publish() {
        var mode = stickerSkin ? 'stickers' : selectedItem ? 'skins' : (category === 'melee' ? 'knives' : category === 'gloves' ? 'gloves' : 'inventory');
        var rows = [];
        if (stickerSkin) {
            rows = stickerCatalog.map(function (entry) { return { id: 'OfflineStickerPick' + entry.sticker, sticker: entry.sticker, name: Name(entry.name), image: entry.image, subtitle: entry.sticker === pendingSticker ? '待确认' : '', selected: entry.sticker === pendingSticker }; });
        } else if (selectedItem) {
            skins.slice(skinPage * pageSize, (skinPage + 1) * pageSize).forEach(function (skin) {
                rows.push({ id: 'OfflineSkin' + skin.paint, definition: selectedItem.definition,
                    paint: skin.paint, name: Name(skin.name), image: skin.image,
                    subtitle: skin.paint === pendingSkin.paint ? '待确认' : (skin.selected ? '已保存' : ''),
                    selected: skin.paint === pendingSkin.paint, sticker_action: skin.sticker_slots > 0 ? 'OfflineStickerOpen' + skin.paint : '' });
            });
        } else {
            rows = InventoryRows();
        }
        var state = { mode: mode, title: selectedItem ? selectedItem.name : '', category: category,
            status: status, items: rows, total: inventory.length, categories: categories.map(function (entry) {
                return { id: 'OfflineCategory_' + entry[0], key: entry[0], name: entry[1], selected: category === entry[0],
                    count: inventory.filter(function (item) { return entry[0] === 'all' || item.kind === entry[0]; }).length };
            }) };
        if (selectedItem) {
            state.skin = { definition: selectedItem.definition, team: team, shared: SharedGun(selectedItem),
                knife: selectedItem.kind === 'melee', glove: selectedItem.kind === 'gloves',
                save_only: SeparateEquip(selectedItem), locked_team: TeamList(), ct: Eligible(selectedItem, 3),
                t: Eligible(selectedItem, 2), name: Name(pendingSkin.name), image: pendingSkin.image,
                total: skins.length, page: skinPage + 1, pages: Math.max(1, Math.ceil(skins.length / pageSize)),
                previous: skinPage > 0, next: (skinPage + 1) * pageSize < skins.length, confirm: !saving };
        }
        if (stickerSkin) {
            var equippedSticker = stickerSlots[stickerSlot] ? stickerSlots[stickerSlot].sticker : 0;
            state.title = '贴纸';
            state.stickers = { paint: stickerSkin.paint, slot: stickerSlot, pending: pendingSticker,
                slots: stickerSlots.map(function (entry) { return { slot: entry.slot, name: Name(entry.name), sticker: entry.sticker, selected: entry.slot === stickerSlot }; }) };
            state.skin.name = Name(stickerSkin.name); state.skin.image = stickerSkin.image;
            state.skin.total = rows.length - 1; state.skin.confirm = !saving; state.skin.previous = state.skin.next = false;
            state.skin.confirm_text = pendingSticker === 0 ? '移除贴纸' : equippedSticker && equippedSticker !== pendingSticker ? '确认替换' : '确认贴纸';
        }
        Root().SetAttributeString('icsm-inventory-state', JSON.stringify(state));
    }
    function DrawActions() {
        var actions = Find('OfflineInventoryNativeActions');
        actions.RemoveAndDeleteChildren();
        inventory.forEach(function (item) {
            [3, 2].forEach(function (value) {
                var button = $.CreatePanel('Button', actions, 'OfflineEquip' + (value === 3 ? 'CT' : 'T') + item.definition);
                button.enabled = Eligible(item, value);
                button.SetPanelEvent('onactivate', function () { EquipTeam(item.definition, value); });
            });
        });
    }
    function DrawInventory() {
        var list = Find('OfflineInventoryItems');
        list.RemoveAndDeleteChildren();
        Root().SetHasClass('knife-split', TeamList());
        InventoryRows().forEach(function (row) {
            var item = FindItem(row.definition), card = $.CreatePanel('Button', list, row.id || '');
            card.AddClass('offline-inventory-card');
            if (row.side) card.AddClass('offline-knife-card');
            if (row.placeholder) { card.enabled = false; card.style.opacity = '0'; return; }
            var image = $.CreatePanel('Image', card, '');
            image.AddClass('offline-inventory-image'); image.SetImage(EngineImage(row.image,row.definition,row.paint)); image.hittest = false;
            var name = $.CreatePanel('Label', card, '');
            name.AddClass('offline-inventory-name'); name.text = item.name; name.hittest = false;
            var label = $.CreatePanel('Label', card, '');
            label.AddClass('offline-inventory-equipped'); label.text = row.subtitle; label.hittest = false;
            card.SetPanelEvent('onactivate', function () { OpenSkins(item, row.side); });
        });
        categories.forEach(function (entry) { Find('OfflineCategory_' + entry[0]).SetHasClass('equipped', category === entry[0]); });
        Find('OfflineInventoryTitle').text = '';
        Publish();
    }
    function Refresh() {
        if (selectedItem) return;
        ReadInventory();
        if (!status) status = '共 ' + inventory.length + ' 件武器、刀具与手套';
        DrawActions(); DrawInventory();
    }
    function UpdateSelection() {
        if (!pendingSkin) return;
        Find('OfflineSkinWeaponImage').SetImage(EngineImage(pendingSkin.image,selectedItem.definition,pendingSkin.paint));
        Find('OfflineSkinSelectionName').text = Name(pendingSkin.name);
        Find('OfflineSkinConfirm').enabled = !saving;
        skins.slice(skinPage * pageSize, (skinPage + 1) * pageSize).forEach(function (skin) {
            var card = Find('OfflineSkinChoices').FindChildTraverse('OfflineSkin' + skin.paint);
            if (!card) return;
            var pending = skin.paint === pendingSkin.paint;
            card.SetHasClass('equipped', pending);
            card.FindChildTraverse('OfflineSkinLabel' + skin.paint).text = pending ? '✓ 待确认' : (skin.selected ? '已保存' : '点击预览');
        });
        Publish();
    }
    function DrawSkinPage() {
        var choices = Find('OfflineSkinChoices');
        choices.RemoveAndDeleteChildren();
        var pages = Math.max(1, Math.ceil(skins.length / pageSize));
        skinPage = Math.max(0, Math.min(skinPage, pages - 1));
        Find('OfflineSkinPageNumber').text = (skinPage + 1) + ' / ' + pages;
        Find('OfflineSkinPrevious').enabled = skinPage > 0;
        Find('OfflineSkinNext').enabled = skinPage + 1 < pages;
        skins.slice(skinPage * pageSize, (skinPage + 1) * pageSize).forEach(function (skin) {
            var card = $.CreatePanel('Button', choices, 'OfflineSkin' + skin.paint);
            card.AddClass('offline-skin-card');
            var image = $.CreatePanel('Image', card, '');
            image.AddClass('offline-skin-image'); image.SetImage(EngineImage(skin.image,selectedItem.definition,skin.paint)); image.hittest = false;
            var name = $.CreatePanel('Label', card, '');
            name.AddClass('offline-skin-name'); name.text = Name(skin.name); name.hittest = false;
            var label = $.CreatePanel('Label', card, 'OfflineSkinLabel' + skin.paint);
            label.AddClass('offline-inventory-equipped'); label.hittest = false;
            if (skin.sticker_slots > 0) {
                var stickerAction = $.CreatePanel('Button', choices, 'OfflineStickerOpen' + skin.paint);
                stickerAction.style.visibility = 'collapse';
                stickerAction.SetPanelEvent('onactivate', function () { OpenStickers(skin); });
            }
            card.SetPanelEvent('onactivate', function () { pendingSkin = skin; UpdateSelection(); Status(SeparateEquip(selectedItem) ? '确认皮肤后保存；返回列表长按装备' : '点击右下角确认后保存并装备'); });
        });
        UpdateSelection();
    }
    function OpenSkins(item, value) {
        selectedItem = item;
        if (value && Eligible(item, value)) team = value;
        if (!Eligible(item, team)) team = Eligible(item, 3) ? 3 : 2;
        skins = NativeInventoryAPI.GetSkins(team, item.definition) || [];
        skins.forEach(function (skin) { skin.image = EngineImage(skin.image,item.definition,skin.paint); });
        pendingSkin = skins[0];
        skins.forEach(function (skin) { if (skin.selected) pendingSkin = skin; });
        if (!pendingSkin) { selectedItem = null; Status('该武器没有可用外观'); return; }
        skinPage = 0;
        skins.forEach(function (skin, i) { if (skin.paint === pendingSkin.paint) skinPage = Math.floor(i / pageSize); });
        Root().SetHasClass('skins-page', true);
        Find('OfflineInventoryItems').RemoveAndDeleteChildren();
        Find('OfflineInventoryTitle').text = item.name;
        Find('OfflineSkinTeam').text = (SharedGun(item) ? '警方／匪徒共用' : (team === 3 ? '警方' : '匪徒')) + ' · ' + item.name;
        Root().SetHasClass('skin-team-locked', SharedGun(item) || TeamList() || !Eligible(item, 3) || !Eligible(item, 2));
        Find('OfflineSkinConfirmText').text = SeparateEquip(item) ? '确认皮肤' : '确认并装备';
        Find('OfflineSkinCT').enabled = Eligible(item, 3); Find('OfflineSkinT').enabled = Eligible(item, 2);
        Find('OfflineSkinHint').text = '共 ' + skins.length + ' 种外观';
        status = SeparateEquip(item) ? '选择并保存该阵营的皮肤；返回列表后长按装备' : (SharedGun(item) ? '通用枪械皮肤同步应用于双方阵营' : '选择外观，确认后保存并装备');
        DrawSkinPage();
    }
    function CloseSkins() {
        stickerSkin = null; stickerSlots = [];
        selectedItem = null; pendingSkin = null; skins = [];
        Find('OfflineSkinChoices').RemoveAndDeleteChildren(); Find('OfflineSkinWeaponImage').SetImage('');
        Root().SetHasClass('skins-page', false);
    }
    function Confirm() {
        if (stickerSkin) { ConfirmSticker(); return; }
        if (!selectedItem || !pendingSkin || saving) return;
        saving = true; Find('OfflineSkinConfirm').enabled = false; Publish();
        var itemName = selectedItem.name, skinName = Name(pendingSkin.name);
        var saveOnly = SeparateEquip(selectedItem), shared = SharedGun(selectedItem);
        var success = saveOnly ? NativeInventoryAPI.SaveSkin(team, selectedItem.definition, pendingSkin.paint) :
            NativeInventoryAPI.EquipSkin(team, selectedItem.definition, pendingSkin.paint);
        saving = false;
        if (success) { CloseSkins(); Refresh(); Status((shared ? '双方皮肤已同步：' : '已为' + (team === 3 ? '警方' : '匪徒') + (saveOnly ? '保存皮肤：' : '装备 ')) + itemName + ' · ' + skinName); }
        else { UpdateSelection(); Status('保存失败，请重试'); }
    }
    function EquipTeam(definition, value) {
        var item = null;
        inventory.forEach(function (entry) { if (entry.definition === definition) item = entry; });
        if (!item || !Eligible(item, value)) return;
        var success = NativeInventoryAPI.Equip(value, item.teams[value].slot, definition);
        if (success) { team = value; Refresh(); Status('已为' + (value === 3 ? '警方' : '匪徒') + '装备 ' + item.name + ' · 已保存'); }
        else Status('装备失败，请重试');
    }
    function ChangeSkinPage(delta) { if (selectedItem) { skinPage += delta; DrawSkinPage(); } }
    function SetSkinTeam(value) { if (selectedItem && !SharedGun(selectedItem) && !TeamList() && Eligible(selectedItem, value)) { var item = selectedItem; team = value; OpenSkins(item); } }
    function SetCategory(value) { CloseSkins(); category = value; Refresh(); Status('共 ' + inventory.filter(function (item) { return category === 'all' || item.kind === category; }).length + ' 件'); }
    function Back() { if (stickerSkin) { stickerSkin = null; status = '长按枪械皮肤管理贴纸'; DrawSkinPage(); return; } CloseSkins(); Refresh(); Status('共 ' + inventory.length + ' 件武器、刀具与手套'); }
    function OpenStickers(skin) {
        if (!selectedItem || !skin || !skin.sticker_slots) return;
        stickerSkin = skin;
        if (!stickerCatalog) stickerCatalog = NativeInventoryAPI.GetStickerCatalog() || [];
        stickerSlots = NativeInventoryAPI.GetStickerSlots(team, selectedItem.definition, skin.paint) || [];
        stickerSlot = 0; pendingSticker = stickerSlots.length ? stickerSlots[0].sticker : 0;
        status = '选择槽位和贴纸后确认；已贴的位置可直接替换，无需先移除'; Publish();
    }
    function ConfirmSticker() {
        if (!stickerSkin || saving) return;
        saving = true;
        var targetSlot = stickerSlot, targetSticker = pendingSticker;
        var previousSticker = stickerSlots[targetSlot] ? stickerSlots[targetSlot].sticker : 0;
        var success = NativeInventoryAPI.SaveSticker(team, selectedItem.definition, stickerSkin.paint, targetSlot, targetSticker);
        saving = false;
        if (success) {
            stickerSlots = NativeInventoryAPI.GetStickerSlots(team, selectedItem.definition, stickerSkin.paint) || [];
            success = !!stickerSlots[targetSlot] && stickerSlots[targetSlot].sticker === targetSticker;
        }
        if (success) {
            status = '位置 ' + (targetSlot + 1) + (targetSticker === 0 ? ' 已移除贴纸' : previousSticker && previousSticker !== targetSticker ? ' 已替换贴纸' : ' 已保存贴纸') + ' · 下次获得对应武器时生效';
        } else status = '贴纸保存失败，请重试';
        Publish();
    }
    function NativeStickerAction(id) {
        if (!stickerSkin) return false;
        var pick = /^OfflineStickerPick([0-9]+)$/.exec(id), slot = /^OfflineStickerSlot([0-9]+)$/.exec(id);
        if (pick) {
            var value = Number(pick[1]);
            for (var i = 0; i < stickerCatalog.length; ++i) if (stickerCatalog[i].sticker === value) {
                pendingSticker = value; status = '位置 ' + (stickerSlot + 1) + ' · ' + Name(stickerCatalog[i].name) + ' · 确认后直接覆盖该位置'; Publish(); return true;
            }
        } else if (slot) {
            var index = Number(slot[1]);
            if (index >= 0 && index < stickerSlots.length) {
                stickerSlot = index; pendingSticker = stickerSlots[index].sticker; status = '位置 ' + (index + 1) + ' · ' + Name(stickerSlots[index].name); Publish(); return true;
            }
        }
        return false;
    }
    // Bounded local development command uses the same panel events as UIKit.
    function ValidateUI(action, id) {
        if (action === 'activate' && NativeStickerAction(id || '')) return;
        var panel = id ? Root().FindChildTraverse(id) : null;
        if (action === 'activate' && panel && panel.enabled && id.indexOf('Offline') === 0)
            $.DispatchEvent('Activated', panel, 'mouse');
        if (action === 'state') $.Msg('ICSM_INVENTORY_STATE ', Root().GetAttributeString('icsm-inventory-state', ''));
    }
    return { Refresh: Refresh, SetCategory: SetCategory, Back: Back, Confirm: Confirm, ValidateUI: ValidateUI,
        ChangeSkinPage: ChangeSkinPage, SetSkinTeam: SetSkinTeam, EquipTeam: EquipTeam };
})();
(function () {
    $.RegisterForUnhandledEvent('MainMenuTabShown', function (tab) { if (tab === 'JsInventory') OfflineInventory.Refresh(); });
    OfflineInventory.Refresh();
})();
