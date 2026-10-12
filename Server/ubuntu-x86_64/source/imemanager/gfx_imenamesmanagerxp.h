/**************************************************************************

Filename    :   GFx_IMENamesManagerXP.h
Content     :   Overrides the functions used to obtain names of installed ime's and 
                activating ime's according to platform specific implementation.
Created     :   Oct 01, 2008
Authors     :   Ankur Mohan

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/

#ifndef _GFX_IMENAMESMANAGERXP_H_
#define _GFX_IMENAMESMANAGERXP_H_

#include "gfx_imeidmap.h"
#include <msctf.h>
#include "gfx_tsfuielementdef.h"

class CIMENamesManagerXP: public CIMENamesManager
{      
public:
    CIMENamesManagerXP( GFxIMEManagerWin32 *pIMEManagerWin32 );
    ~CIMENamesManagerXP();

    virtual bool		QualifyIMENames();   
    virtual void        ActivateIME( const wchar_t *pIMEName );
    void                ActivateInputLanguage( const wchar_t *pInputLangName );
    virtual void        HandleStatusWindowNotifications( const char *pCommand, const char *pArg );
    virtual void        OnLangBarLoaded();
	void				CleanUp();

private:
	virtual int			CheckForSupportedIME( const wchar_t *pLayoutTextName, const wchar_t *pIMEFileName );
};

#endif
