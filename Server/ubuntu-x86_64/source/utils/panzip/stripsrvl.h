#ifndef __STRIP_SRVL__
#define __STRIP_SRVL__

enum SSResult_t
{
	SS_OK,
	SS_UNKNOWN_FILE_TYPE,
	SS_BAD_FORMATTING,
	SS_ERROR_OPEN_INPUT_FILE,
	SS_ERROR_CREATE_OUTPUT_FILE,
};

static const char *SSResultAsString( SSResult_t res )
{
	switch( res )
	{
	case SS_OK: return "SS_OK";
	case SS_UNKNOWN_FILE_TYPE: return "SS_UNKNOWN_FILE_TYPE";
	case SS_BAD_FORMATTING: return "SS_BAD_FORMATTING";
	case SS_ERROR_OPEN_INPUT_FILE: return "SS_ERROR_OPEN_INPUT_FILE";
	case SS_ERROR_CREATE_OUTPUT_FILE: "SS_ERROR_CREATE_OUTPUT_FILE";
	default:
		return "UNKNOWN";
	}
}

SSResult_t StripSrvl( const char *pSrcFilePath, const char *pDestFilePath );

#endif	// __STRIP_SRVL__