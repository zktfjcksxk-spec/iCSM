'use strict';


var SettingsMenuShared = ( function () {


	var _ResetControlsRecursive = function( panel )
	{
		if ( panel == null || panel.id == "ICSMVideoInternal" )
		{
			return;
		}

		if (panel.GetChildCount == undefined)
		{
			                                       
			return;
		}

		if (panel.paneltype == 'CSGOSettingsSlider' || panel.paneltype == 'CSGOSettingsEnumDropDown')
		{
			panel.RestoreCVarDefault();
		}
		else if ( panel.paneltype == 'CSGOSettingsKeyBinder' )
		{
			                                                                                          
			panel.OnShow();
		}
		else                                          
		{
			var nCount = panel.GetChildCount();
			for ( var i = 0; i < nCount; i++ )
			{
				var child = panel.GetChild(i);
				_ResetControlsRecursive(child);
			}
		}
	};

    var _FindSettingsTab = function (id)
    {
        for (var panel = $.GetContextPanel(); panel; panel = panel.GetParent()) {
            if (panel.id === id) return panel;
            var child = panel.FindChildTraverse(id);
            if (child) return child;
        }
        return null;
    };

    var _ResetControls = function (tab)
    {
        // Several pages share a JS context. Reset only the selected category.
        var id = tab || SettingsMenu.GetActiveTab();
        _ResetControlsRecursive(_FindSettingsTab(id));
    };

	var _ResetKeybdMouseDefaults = function( )
	{
		                                 
		OptionsMenuAPI.RestoreKeybdMouseBindingDefaults();
		_ResetControls();
	};

    var _ResetControllerDefaults = function ()
    {
        GameInterfaceAPI.ConsoleCommand("icsm_controller_reset");
        $.Schedule(0.25, function () { _RefreshControlsRecursive(_FindSettingsTab("ControllerSettings")); });
    };

	var _ResetAudioSettings = function( )
	{
		// Remaining audio controls reset their own ConVars.
		_ResetControls("AudioSettings");
	};

    var _ResetVideoSettings = function ()
    {
        // Quality is owned by the defined preset. Independent options keep
        // their saved values; no hardware autodetection or global reset.
        GameInterfaceAPI.ConsoleCommand("icsm_apply_quality_preset");
    };

	var _RefreshControlsRecursive = function( panel )
	{
		if ( panel == null )
		{
			return;
		}

		if ( panel.OnShow != undefined )
		{
			panel.OnShow();
		}
		
		if (panel.GetChildCount == undefined)
		{
			                                       
			return;
		}
		else                                          
		{
			var nCount = panel.GetChildCount();
			for ( var i = 0; i < nCount; i++ )
			{
				var child = panel.GetChild(i);
				_RefreshControlsRecursive(child);
			}
		}
	};

	var _ShowConfirmReset = function ( resetCall, locText )
	{
		UiToolkitAPI.ShowGenericPopupTwoOptionsBgStyle('#settings_reset_confirm_title',
			locText,
			'',
			'#settings_reset',
			function() {
				resetCall();
			},
			'Return',
			function() {
			},
			'dim'
		);
	}

	var _ShowConfirmDiscard = function ( discardCall )
	{
		UiToolkitAPI.ShowGenericPopupTwoOptionsBgStyle('#settings_discard_confirm_title',
			'#settings_discard_confirm_video_desc',
			'',
			'#settings_discard',
			function() {
				discardCall();
			},
			'Return',
			function() {
			},
			'dim'
		);
	}

	var _ScrollToId = function ( locationId )
	{
		var elLocationPanel = $.GetContextPanel().FindChildTraverse( locationId );

		if ( elLocationPanel != null )
		{
			elLocationPanel.ScrollParentToMakePanelFit(3, false);
			elLocationPanel.AddClass('Highlight');

			var kfs = elLocationPanel.CreateCopyOfCSSKeyframes( 'settings-highlight' );
			elLocationPanel.UpdateCurrentAnimationKeyframes( kfs );
		}
	}
	
	                                                                    
	                                           
	                                      
	                                       
	                                         

	var gBtnApplyVideoSettingsButton=null;
	var gBtnDiscardVideoSettingChanges=null;
	
	var _VideoSettingsOnUserInputSubmit = function ()
	{
		if ( gBtnApplyVideoSettingsButton != null )
		{
			gBtnApplyVideoSettingsButton.enabled = true;
		}

		if ( gBtnDiscardVideoSettingChanges != null )
		{
			gBtnDiscardVideoSettingChanges.enabled = true;
		}
	}

	var _VideoSettingsResetUserInput = function ()
	{
		if ( gBtnApplyVideoSettingsButton != null )
		{
			gBtnApplyVideoSettingsButton.enabled = false;
		}

		if ( gBtnDiscardVideoSettingChanges != null )
		{
			gBtnDiscardVideoSettingChanges.enabled = false;
		}
	}

	var _VideoSettingsDiscardChanges = function ()
	{
		$.DispatchEvent( "CSGOVideoSettingsInit" );
        _LockIOSVideoControls();
		_VideoSettingsResetUserInput();
	}

    // Source requires these controls, but they are not player settings now.
    var _LockIOSVideoControls = function ()
    {
        var internal = $.GetContextPanel().FindChildTraverse("ICSMVideoInternal");
        if (!internal) return;
        function disable(panel) {
            if (panel.paneltype === "CSGOSettingsEnumDropDown" || panel.paneltype === "CSGOSettingsSlider") panel.enabled = false;
            if (panel.GetChildCount) for (var i = 0; i < panel.GetChildCount(); ++i) disable(panel.GetChild(i));
        }
        disable(internal);
    };

    var _VideoSettingsApplyChanges = function ()
    {
        // Do not apply the original hidden dropdowns: only the independent
        // controls write their own ConVars, and the preset owns everything else.
        GameInterfaceAPI.ConsoleCommand("icsm_apply_quality_preset");
        _VideoSettingsResetUserInput();
    };

	var _NewTabOpened = function ( newTab )
	{
		                                            
		
		var videoSettingsStr = 'VideoSettings';

		if ( newTab == videoSettingsStr )
		{
			var videoSettingsPanel = $.GetContextPanel().FindChildInLayoutFile( videoSettingsStr );
			
			                                                                 
			gBtnApplyVideoSettingsButton = videoSettingsPanel.FindChildInLayoutFile( "BtnApplyVideoSettings" );
			gBtnDiscardVideoSettingChanges = videoSettingsPanel.FindChildInLayoutFile( "BtnDiscardVideoSettingChanges" );
			
			                                  
			gBtnApplyVideoSettingsButton.enabled = false;
			gBtnDiscardVideoSettingChanges.enabled = false;

			                                         
			$.DispatchEvent( "CSGOVideoSettingsInit" );
        _LockIOSVideoControls();
		}

		var newTabPanel = $.GetContextPanel().FindChildInLayoutFile( newTab );
		_RefreshControlsRecursive( newTabPanel );

		                                                                               
		GameInterfaceAPI.ConsoleCommand( "host_writeconfig");
	}

	var _ShowHudEdgePositions = function()
	{
		UiToolkitAPI.ShowCustomLayoutPopup('', 'file://{resources}/layout/popups/popup_hud_edge_positions.xml');
	}


	return {

		ResetControlsRecursivepanel	    : _ResetControlsRecursive,
		ResetControls	                : _ResetControls,
		ResetKeybdMouseDefaults	        : _ResetKeybdMouseDefaults,
        ResetControllerDefaults         : _ResetControllerDefaults,
		ResetAudioSettings	            : _ResetAudioSettings,
		ResetVideoSettings	            : _ResetVideoSettings,
		ScrollToId                      : _ScrollToId,
		ShowConfirmReset                : _ShowConfirmReset,
		ShowConfirmDiscard				: _ShowConfirmDiscard,
		VideoSettingsOnUserInputSubmit  : _VideoSettingsOnUserInputSubmit,
		VideoSettingsDiscardChanges		: _VideoSettingsDiscardChanges,
		VideoSettingsApplyChanges		: _VideoSettingsApplyChanges,
		NewTabOpened					: _NewTabOpened,
		ShowHudEdgePositions			: _ShowHudEdgePositions
	};

} )();
