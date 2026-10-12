//
// UI component functions for player resource and other gameplay data 
//

UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsLatched, "", "Is gamestate data currently latched and not updating (e.g. end-of-match)" )

// PLAYER DATA
//
//

UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerIndex, "XUID", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerName, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerNameSafe, "XUID", "Deprecated, use GetPlayerName() instead" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerNameWithNoHTMLEscapes, "XUID", "Don't use this, set html=true on your label and use GetPlayerName()" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerXuidStringFromEntIndex, "EntIndex", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerXuidFromUserID, "UserID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetLocalPlayerXuid, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerTeamName, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerTeamNumber, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetCoachingTeamNumber, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetAssociatedTeamNumber, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsPlayerConnected, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsPlayerAlive, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerPing, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerKills, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerAssists, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerDeaths, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerMVPs, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerGungameLevel, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerMoney, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerScore, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerClanTag, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerActiveCoinRank, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerCompetitiveRanking, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerCompetitiveWins, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerDisplayFlairItem, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerStatus, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsFakePlayer, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerActiveWeaponItemId, "XUID", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsXuidValid, "XUID", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( float, GetPlayerVoiceVolume, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, SetPlayerVoiceVolume, "XUID value", "" )


UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetMapsInMapGroup, "string", "Return a comma delimited list of short mapnames (eg 'de_dust2, de_inferno, 123123/de_workshopmap') in the given map group." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetMapDisplayNameToken, "string", "Return a loc token for the given short map name (eg 'de_dust2')." )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetMapsInCurrentMapGroup, "string", "Return a comma delimited list of short mapnames (eg 'de_dust2, de_inferno, 123123/de_workshopmap') in the currently playing map group (returns null if not in game)." )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerLifetime, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerColor, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerXpLevel, "XUID", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerCommendsLeader, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerCommendsTeacher, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetPlayerCommendsFriendly, "XUID", "" )

// Survival Specific
UI_COMPONENT_FUNCTIONLIST_ELEMENT( float, GetSurvivalTimeUntilNextWave, "", "" )

// Ported methods of sfhudcallvotepanel
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsQueuedMatchmaking, "", "Return true if this match was entered " )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsEndMatchMapVoteEnabled, "", "If we should show map vote at end of match" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW( Object, GetKickTargets, "Get array of objects representing kickable players for display in a popup." );

//
//
//// OTHER DATA
////


UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, ArePlayersEnemies, "XUID1, XUID2", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsLocalPlayerPlayingMatch, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, AreTeamsPlayingSwitchedSides, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, AreTeamsPlayingSwitchedSidesInRound, "round", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsReportCategoryEnabledForSelectedPlayer, "XUID, Category", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, SubmitPlayerReport, "XUID, Category", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, SubmitServerReport, "Category", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, QueryServersForCommendation, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool,GetCommendationTokensAvailable, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW( object, GetMyCommendationsJSOForUser, "XUID" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool,SubmitCommendation, "XUID, CommendationString", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsSelectedPlayerMuted, "XUID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( void, ToggleMute, "XUID", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsDemoOrHltv, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsLocalPlayerHLTV, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, IsHLTVAutodirectorOn, "", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, SetCasterIsCameraman, "int nAccountID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, SetCasterIsHeard, "int nAccountID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, SetCasterControlsXray, "int nAccountID", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, SetCasterControlsUI, "int nAccountID", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetServerName, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetMapName, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetTournamentEventStage, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetGameModeInternalName, "bool bUseSkirmishName", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetGameModeImagePath, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetGameModeName, "bool bUseSkirmishName", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetViewerCount, "", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetMapBSPName, "", "Returns the name of the bsp file eg 'de_dust2'" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( bool, HasHalfTime, "", "" )


UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetPlayerModel, "", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW( Object, GetPlayerDataJSO, "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW( Object, GetTimeDataJSO, "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW( Object, GetScoreDataJSO, "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT_RAW( Object, GetMatchEndWinDataJSO, "" )




//
//
// Team data
//
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string,	GetTeamClanName, "strTeamName", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string,	GetTeamLogoImagePath, "strTeamName", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string,	GetTeamFlagImagePath, "strTeamName", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( string, GetTeamGungameLeaderXuid, "strTeamName", "" )

UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetTeamTotalPlayerCount, "strTeamName", "" )
UI_COMPONENT_FUNCTIONLIST_ELEMENT( int, GetTeamLivingPlayerCount, "strTeamName", "" )
