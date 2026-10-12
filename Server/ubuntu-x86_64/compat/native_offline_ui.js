// Availability describes this accountless build. No online account data is
// synthesized. Local settings and map launch come from the native Source APIs.
var NativeOfflineUI = {
    Unavailable: function () {
        UiToolkitAPI.ShowGenericPopupOk('离线模式', '此原生版本提供本地地图与机器人对战；Steam、库存、官方匹配和在线服务不可用。', '', function () {});
    }
};
var LobbyAPI = {
    IsSessionActive: function () { return NativeOfflineAPI.IsSessionActive(); },
    CreateSession: function () { NativeOfflineAPI.CreateSession(); },
    GetSessionSettings: function () { return NativeOfflineAPI.GetSessionSettings(); },
    UpdateSessionSettings: function (settings) { NativeOfflineAPI.UpdateSessionSettings(settings); },
    BIsHost: function () { return NativeOfflineAPI.IsSessionActive(); },
    StartMatchmaking: function () { return NativeOfflineAPI.StartLocalMatch(); },
    StopMatchmaking: function () {}, // Local launch has no matchmaking queue.
    GetMatchmakingStatusString: function () { return ''; },
    GetTimeSpentMatchmaking: function () { return 0; },
    GetMapWaitTimeInSeconds: function () { return 0; }
};
var GameTypesAPI = {
    GetConfig: function () { return NativeOfflineAPI.GetConfig(); },
    GetMapGroupAttribute: function (group, key) {
        var entry = NativeOfflineAPI.GetConfig().mapgroups[group];
        return entry && entry[key] !== undefined ? entry[key] : '';
    },
    SetCustomBotDifficulty: function (value) { return NativeOfflineAPI.SetCustomBotDifficulty(Number(value)); },
    GetCustomBotDifficulty: function () { return NativeOfflineAPI.GetCustomBotDifficulty(); }
};
var MyPersonaAPI = {
    GetLauncherType: function () { return 'native-offline'; },
    IsInventoryValid: function () { return false; },
    IsConnectedToGC: function () { return false; },
    GetMyOfficialTeamName: function () { return ''; },
    GetMyOfficialTournamentName: function () { return ''; },
    GetName: function () { return NativeOfflineAPI.GetPlayerName(); },
    GetXuid: function () { return NativeOfflineAPI.GetLocalPlayerId(); }
};
var PartyListAPI = {
    GetLocalPlayerForHireAdvertising: function () { return ''; },
    GetCount: function () { return 0; },
    GetPartySystemSetting: function () { return ''; },
    GetPartyMemberSetting: function (id, key) {
        if (key === 'game/teamcolor') {
            var value = NativeOfflineAPI.GetPlayerTeamColor(id);
            return value >= 0 ? String(value) : '';
        }
        return '';
    },
    GetFriendIsTalking: function () { return false; }
};
var CompetitiveMatchAPI = { HasOngoingMatch: function () { return false; } };
var SteamOverlayAPI = { IsEnabled: function () { return false; }, OpenURL: NativeOfflineUI.Unavailable };
var MatchStatsAPI = {
    GetUiExperienceType: function () { return ''; },
    IsConnectedToCommunityServer: function () { return false; },
    IsTournamentMatch: function () { return NativeOfflineAPI.IsTournamentMatch(); },
    GetPlayerStatsJSO: function (id) { return NativeOfflineAPI.GetPlayerStats(id); },
    DoesSupportOvertimeStats: function () { return false; }, // Replicated totals have no separate overtime buckets.
    GetServerWebsiteURL: function () { return ''; } // This local server has no website.
};
