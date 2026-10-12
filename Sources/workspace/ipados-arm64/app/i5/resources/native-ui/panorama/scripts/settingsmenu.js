"use strict";

var SettingsMenu = (function () {
    var activeTab;
    var tabInfo = {
        VideoSettings: { xml: "settings_video", radioid: "VideoRadio" },
        TouchSettings: { xml: "settings_touch", radioid: "TouchRadio" },
        GameSettings: { xml: "settings_game", radioid: "GameRadio" },
        AudioSettings: { xml: "settings_audio", radioid: "AudioRadio" },
        KeybdMouseSettings: { xml: "settings_kbmouse", radioid: "KBMouseRadio" },
        ControllerSettings: { xml: "settings_controller", radioid: "ControllerRadio" }
    };

    function NavigateToTab(tabID) {
        if (!tabInfo[tabID]) return;
        var parent = $("#SettingsMenuContent");
        var panel = parent.FindChildInLayoutFile(tabID);
        if (!panel) {
            panel = $.CreatePanel("Panel", parent, tabID);
            panel.BLoadLayout("file://{resources}/layout/settings/" + tabInfo[tabID].xml + ".xml", false, false);
            panel.visible = false;
        }
        if (activeTab !== tabID) {
            if (activeTab) {
                var previous = parent.FindChildInLayoutFile(activeTab);
                previous.RemoveClass("Active");
                previous.visible = false;
                previous.SetReadyForDisplay(false);
            }
            activeTab = tabID;
            parent.SetAttributeString("icsm-active-settings-tab", tabID);
            panel.visible = true;
            panel.SetReadyForDisplay(true);
            panel.AddClass("Active");
            var radio = $("#" + tabInfo[tabID].radioid);
            if (radio) radio.checked = true;
            SettingsMenuShared.NewTabOpened(tabID);
            $.Msg("ICSM_SETTINGS_TAB ", tabID);
        }
    }

    function NavigateToSetting(tabID, id) {
        if (!tabInfo[tabID]) return;
        NavigateToTab(tabID);
        SettingsMenuShared.ScrollToId(id);
    }

    // Invoked only by the local i5_settings_ui development command.
    function ValidateUI(action, id, option) {
        var root = $.GetContextPanel();
        if (action === "tab") { NavigateToTab(id); return; }
        if (action === "play") {
            var main = root.Data().elMainMenuRoot;
            var play = main ? main.FindChildTraverse("MainMenuNavBarPlay") : null;
            if (play && play.enabled) $.DispatchEvent("Activated", play, "mouse");
            return;
        }
        var activePanel = root.FindChildTraverse(activeTab);
        var control = id && activePanel ? activePanel.FindChildTraverse(id) : null;
        if (activeTab === "VideoSettings" && control && action !== "state") {
            var allowed = { ICSMFrameRateLimit: true, MotionBlur: true, ICSMSkinShaderDetail: true,
                            ICSMSkinTextureDetail: true, ICSMQualityHigh: true, ICSMQualityVeryLow: true,
                            ICSMQualityLow: true, ICSMQualityMedium: true, ICSMFrameInterpolation: true, ICSMFrameCalibration: true };
            if (!allowed[id] || !control.enabled) { $.Msg("ICSM_SETTINGS_LOCKED ", id); return; }
        }
        if (action === "reset" && activeTab === "AudioSettings") {
            SettingsMenuShared.ResetAudioSettings();
        } else if (action === "select" && control && control.paneltype === "CSGOSettingsEnumDropDown" && control.HasOption(option)) {
            // The menu activation path also emits onuserinputsubmit, just
            // like choosing an option by hand; SetSelected alone does not.
            $.DispatchEvent("Activated", control, "mouse");
            $.DispatchEvent("Activated", control.FindDropDownMenuChild(option), "mouse");
        } else if (action === "slider" && control && control.paneltype === "CSGOSettingsSlider") {
            var value = Number(option);
            if (isFinite(value) && value >= Math.min(control.min, control.max) && value <= Math.max(control.min, control.max)) control.value = value;
        } else if (action === "activate" && control && control.enabled) {
            $.DispatchEvent("Activated", control, "mouse");
        } else if (action === "state") {
            var state = { tab: activeTab, controls: [], layout: [] };
            [root, root.FindChildTraverse("SettingsMenuContent"), root.FindChildTraverse(activeTab),
                activePanel ? activePanel.FindChildTraverse("ICSMSettingsScrollViewport") : null].forEach(function(panel) {
                if (panel) state.layout.push({ id: panel.id, width: panel.actuallayoutwidth, height: panel.actuallayoutheight,
                    x: panel.actualxoffset, y: panel.actualyoffset, scale: panel.actualuiscale_x, scrollY: panel.scrolloffset_y });
            });
            function visit(panel, hidden) {
                hidden = hidden || panel.id === "ICSMVideoInternal" || panel.id === "ICSMVideoActionsInternal";
                if (panel.paneltype === "CSGOSettingsEnumDropDown") {
                    var selected = panel.GetSelected();
                    state.controls.push({ id: panel.id, type: panel.paneltype, hidden: hidden, enabled: panel.enabled, selected: selected ? selected.id : "" });
                } else if (panel.paneltype === "CSGOSettingsSlider") {
                    var title = panel.FindChildTraverse("Title");
                    var row = title ? title.GetParent() : null;
                    state.controls.push({ id: panel.id, type: panel.paneltype, hidden: hidden, enabled: panel.enabled, value: panel.ActualValue(),
                        title: title ? { text: title.text, width: title.actuallayoutwidth, height: title.actuallayoutheight } : null,
                        row: row ? { width: row.actuallayoutwidth, height: row.actuallayoutheight } : null });
                }
                if (panel.GetChildCount) for (var i = 0; i < panel.GetChildCount(); ++i) visit(panel.GetChild(i), hidden);
            }
            visit(root.FindChildTraverse(activeTab), false);
            var controls = state.controls;
            state.control_count = controls.length;
            delete state.controls;
            $.Msg("ICSM_SETTINGS_STATE ", JSON.stringify(state));
            controls.forEach(function(control) { control.tab = activeTab; $.Msg("ICSM_SETTINGS_CONTROL ", JSON.stringify(control)); });
        }
    }

    function OnSettingsMenuHidden() {
        GameInterfaceAPI.ConsoleCommand("host_writeconfig");
    }

    return {
        NavigateToTab: NavigateToTab,
        ValidateUI: ValidateUI,
        GetActiveTab: function () { return activeTab; },
        NavigateToSetting: NavigateToSetting,
        OnSettingsMenuHidden: OnSettingsMenuHidden
    };
})();

(function () {
    SettingsMenu.NavigateToTab("VideoSettings");
    $.RegisterEventHandler("UnreadyForDisplay", $("#JsSettings"), SettingsMenu.OnSettingsMenuHidden);
    $.RegisterForUnhandledEvent("SettingsMenu_NavigateToSetting", SettingsMenu.NavigateToSetting);
})();
