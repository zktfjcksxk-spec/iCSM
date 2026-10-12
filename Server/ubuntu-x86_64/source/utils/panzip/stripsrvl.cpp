#include "stripsrvl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SSResult_t StripSrvl( const char *pSrcFilePath, const char *pDestFilePath )
{
	FILE *fpIn = fopen( pSrcFilePath, "r" );
	if ( !fpIn )
	{
		return SS_ERROR_OPEN_INPUT_FILE;
	}

	// See what kind of file we have
	char ext[_MAX_EXT];
	char *pBlockStartStr = nullptr;
	char *pBlockEndStr = nullptr;

	_splitpath(pSrcFilePath, nullptr, nullptr, nullptr, ext);

	if ( !_stricmp( ext, ".js" ) )
	{
		pBlockStartStr = "//SRVL{";
		pBlockEndStr = "//}SRVL";
	}
	else if( !_stricmp( ext, ".xml" ) )
	{
		pBlockStartStr = "<!--SRVL{-->";
		pBlockEndStr = "<!--}SRVL-->";
	} 
	else if( !_stricmp( ext, ".css" ) )
	{
		pBlockStartStr = "/*SRVL{*/";
		pBlockEndStr = "/*}SRVL*/";
	} 
	else
	{
		fclose( fpIn );
		return SS_UNKNOWN_FILE_TYPE;
	}

	// Open output file
	FILE *fpOut = fopen( pDestFilePath, "w" );
	if ( !fpOut )
	{
		fclose( fpIn );
		return SS_ERROR_CREATE_OUTPUT_FILE;
	}

	char buff[ 1024 ];
	int level = 0;

	while ( fgets( buff, sizeof(buff), fpIn ) )
	{
		bool bMarkup = false;

		if ( strstr( buff, pBlockStartStr ) )
		{
			bMarkup = true;
			level++;
		}
		else if ( strstr( buff, pBlockEndStr ) )
		{
			bMarkup = true;
			level--;
		}
		
		if ( level != 0 || bMarkup )
		{
			fputs( "\n", fpOut );
		}
		else 
		{
			fputs( buff, fpOut );
		}
	}

	fclose ( fpOut );
	fclose ( fpIn );

	if(level != 0)
	{
		return SS_BAD_FORMATTING;
	}
	else
	{
		return SS_OK;
	}
}