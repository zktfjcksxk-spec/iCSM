#ifndef __STRIP_BLOCKS__
#define __STRIP_BLOCKS__

class KeyValues;

struct StringAndLen_t
{
	const char *pStr;
	unsigned int nStrLen;
};

class CStringsArray
{
public:

	void AddString( const char *pStr );

	int NumStrings() { return m_vec.Count(); };
	StringAndLen_t const* Get( int i ) { return &m_vec[i]; }

	void Free();
	void Log();

	~CStringsArray() { Free(); }

private:
	CUtlVector< StringAndLen_t > m_vec;
};

class CBlockDef
{
public:

	CBlockDef() : m_pName(nullptr) {}

	bool m_bNoFurtherNesting;
	const char *m_pName;
	CStringsArray m_tokens[2];

	void Log();
	void Free();

	enum TokensType_t
	{
		START_TOKENS = 0,
		END_TOKENS = 1,
		NUM_TOKEN_TYPES
	};
};

bool BlockDefsFromKeyValues( CBlockDef **ppBlockDefs, int *pNumBlockDefs, KeyValues *pKV );
bool StripBlocks( const char *pSrcFilePath, const char *pDestFilePath, CBlockDef *pBlockDefs, int nNumBlockDefs );

#endif	// __STRIP_BLOCKS__