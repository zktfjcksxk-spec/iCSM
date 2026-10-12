//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Encapsulates real world (wall clock) time
//
//=============================================================================
#ifndef RTIME_H
#define RTIME_H
#ifdef _WIN32
#pragma once
#endif


#ifdef WIN32
char* strptime(const char *s, const char *format, struct tm *tm);
#endif

#include <time.h>
#include <ctype.h>
#include <string.h>



class CSTime;

// Invalid time values
const RTime32 k_RTime32Nil = 0;
// time values between Nil and MinValid are available for special constants
const RTime32 k_RTime32MinValid = 10;
// infinite time value
const RTime32 k_RTime32Infinite = 0x7FFFFFFF; //01-18-2038

// Flags for component fields. Longer durations must be > shorter ones
// WARNING: DO NOT RENUMBER EXISTING VALUES - STORED IN DATABASE
enum ETimeUnit
{
	k_ETimeUnitNone = 0,
	k_ETimeUnitSecond = 1,
	k_ETimeUnitMinute = 2,
	k_ETimeUnitHour = 3,
	k_ETimeUnitDay = 4,
	k_ETimeUnitWeek = 5,
	k_ETimeUnitMonth = 6,
	k_ETimeUnitYear = 7,
	k_ETimeUnitForever
};

// CRTimeDiff represents a duration.  To support DST changes, differing numbers of days per month,
// etc., it represents each of these durations separately.
class CRTimeDuration
{
public:
	CRTimeDuration() { MakeZero(); }
	CRTimeDuration( int amount, ETimeUnit unit ) { MakeZero(); TimeAdd( amount, unit ); }
	explicit CRTimeDuration( const char* szDurationString ) { ParseDuration( szDurationString ); }

	void MakeZero() {
		for ( int i = 0; i < kNumDiff; ++i )
			m_nDiffs[i] = 0;
	}

	bool operator==( const CRTimeDuration &diff ) const {
		for ( int i = 0; i < kNumDiff; ++i )
		{
			if ( m_nDiffs[i] != diff.m_nDiffs[i] )
				return false;
		}
		return true;
	}
	bool operator!=( const CRTimeDuration &diff ) const {
		for ( int i = 0; i < kNumDiff; ++i )
		{
			if ( m_nDiffs[i] != diff.m_nDiffs[i] )
				return true;
		}
		return false;
	}

	// We can't do comparison operators because it is unclear, for example,
	// if CRTimeDiff("30D12H") is bigger or smaller than CRTimeDiff("1M").

	// Returns true if the duration is a constant number of seconds
	bool IsFixedLength() {
		for ( int i = 0; i < kDiffDays; ++i )
		{
			if ( m_nDiffs[i] != 0 )
				return false;
		}

		return true;
	}
	int32 ToSeconds() { Assert( IsFixedLength() ); return m_nDiffs[kDiffDays] * (60*60*24) + m_nDiffs[kDiffHours] * (60*60) + m_nDiffs[kDiffMinutes] * 60 + m_nDiffs[kDiffSeconds]; }

	CRTimeDuration& operator+=( const CRTimeDuration &diff ) {
		for ( int i = 0; i < kNumDiff; ++i )
			m_nDiffs[i] += diff.m_nDiffs[i];
		return *this;
	}
	CRTimeDuration& operator-=( const CRTimeDuration &diff ) {
		for ( int i = 0; i < kNumDiff; ++i )
			m_nDiffs[i] -= diff.m_nDiffs[i];
		return *this;
	}
	CRTimeDuration& operator*=( int scale ) {
		for ( int i = 0; i < kNumDiff; ++i )
			m_nDiffs[i] *= scale;
		return *this;
	}

	// negate
	CRTimeDuration operator-()
	{
		CRTimeDuration result;
		for ( int i = 0; i < kNumDiff; ++i )
			result.m_nDiffs[i] = -m_nDiffs[i];

		return result;
	}

	CRTimeDuration operator+( const CRTimeDuration &diff ) const { CRTimeDuration result( *this ); result += diff; return result; }
	CRTimeDuration operator-( const CRTimeDuration &diff ) const { CRTimeDuration result( *this ); result -= diff; return result; }
	CRTimeDuration operator*( int scale ) const { CRTimeDuration result( *this ); result *= scale; return result; }

	void TimeAdd( int num, ETimeUnit unit );

	bool ParseDurationISO8601( const char *szDuration );				// Parses a duration in ISO8601 format
	bool ParseDurationHoursMinutesSeconds( const char *szDuration );	// Parses a duration in HH:MM:SS format (with each component optional)
	bool ParseDuration( const char *szDuration );						// Chooses between the above two based on whether the string has a ":"

	// Adds this difference to the tm, normalizes it, and returns the time_t that results.
	time_t ApplyToTm( struct tm *pTm ) const;

	// Returns number of periods of this duration between start and target
	// (rounded towards 0) via binary search.
	//
	// Note: Only works for positive durations.  (However, it is valid for
	//       target < start and can return negative in that case)
	int Divide( RTime32 start, RTime32 target ) const;

	// Display as a string to a temporary buffer
	const char* Render() const;

private:
	enum { kDiffYears, kDiffMonths, kDiffDays, kDiffHours, kDiffMinutes, kDiffSeconds, kNumDiff };
	int32 m_nDiffs[kNumDiff];
};

// CRTime
// This is our primary real time structure.  
// It offers 1 second resolution beginning on January 1, 1970 (i.e unix time)
// This represents wall clock time
class CRTime
{
public:
	// default constructor initializes to the current time
	CRTime();
	CRTime( RTime32 nTime ) : m_nStartTime( nTime ), m_bGMT( false ) {}

	void SetToCurrentTime() { m_nStartTime = sm_nTimeCur; }
	void SetFromCurrentTime( int dSecOffset ) { m_nStartTime = sm_nTimeCur + dSecOffset; }

	// Amount of seconds that have passed between this CRTime being set and the current wall clock time.
	int	CSecsPassed() const;

	// Time accessors
	RTime32 GetRTime32() const { return m_nStartTime; }
	void SetRTime32( RTime32 rTime32 ) { m_nStartTime = rTime32; }

	// Access our cached current time value
	static void UpdateRealTime();
	static void SetSystemClock( RTime32 nCurrentTime );
	static RTime32 RTime32TimeCur() { return sm_nTimeCur; }

	// Render
	const char *Render() const;
	static const char *Render( const RTime32 rTime32 );

	// Return a representation of the current system time
	static char *PchTimeCur() { return sm_rgchLocalTimeCur; }
	// Return a representation of the current system date
	static char *PchDateCur() { return sm_rgchLocalDateCur; }

	// comparisons against other CRTime objects
	bool operator==( const CRTime &val ) const { return val.m_nStartTime == m_nStartTime; }
	bool operator<( const CRTime &val ) const { return m_nStartTime < val.m_nStartTime; }
	bool operator<=( const CRTime &val ) const { return m_nStartTime <= val.m_nStartTime; }
	bool operator>( const CRTime &val ) const { return m_nStartTime > val.m_nStartTime; }
	bool operator>=( const CRTime &val ) const { return m_nStartTime >= val.m_nStartTime; }

	// comparisons against RTime32 numbers (to avoid implicit construct/destruct of a temporary CRTime)
	bool operator==( const RTime32 &val ) const { return m_nStartTime == val; }
	bool operator<( const RTime32 &val ) const { return m_nStartTime < val; }
	bool operator<=( const RTime32 &val ) const { return m_nStartTime <= val; }
	bool operator>( const RTime32 &val ) const { return m_nStartTime > val; }
	bool operator>=( const RTime32 &val ) const { return m_nStartTime >= val; }

	const CRTime& operator=( const RTime32 &val )  { m_nStartTime = val; return *this; }
	const CRTime& operator=( const CRTime &val )  { m_nStartTime = val.m_nStartTime; return *this; }
	const CRTime operator+( const int &nVal )  { return m_nStartTime + nVal; }
	// Add exactly X seconds to this time
	const CRTime& operator+=( const int &nVal )  { m_nStartTime += nVal; return *this; }

	// Component Details
	int GetYear() const;
	int GetMonth() const;		// returns 0..11
	int GetDayOfYear() const;
	int GetDayOfMonth() const;
	int GetDayOfWeek() const;
	int GetHour() const;
	int GetMinute() const;
	int GetSecond() const;
	int GetISOWeekOfYear() const;

	// Handy references to nearby time boundaries
	static RTime32 RTime32BeginningOfDay( const RTime32, bool bGMT ); // at 00:00:00 or 00:00:00 GMT
	static RTime32 RTime32BeginningOfNextDay( const RTime32 );		// at 00:00:00
	static RTime32 RTime32FirstDayOfMonth( const RTime32 );			// at 00:00:00
	static RTime32 RTime32LastDayOfMonth( const RTime32 );			// at 00:00:00
	static RTime32 RTime32FirstDayOfNextMonth( const RTime32 );		// at 00:00:00
	static RTime32 RTime32LastDayOfNextMonth( const RTime32 );		// at 00:00:00

	static bool BIsLeapYear( int nYear );
	bool BIsLeapYear() const { return CRTime::BIsLeapYear( GetYear() ); }

	// Parse time using a format string with strptime format
	static RTime32 RTime32FromFmtString( const char *pchFmt, const char* pchValue );

	// Parse time from string in standard HTTP date format
	static RTime32 RTime32FromHTTPDateString( const char* pchValue );

	// Parse time from string RFC3339 format (assumes UTC)
	static RTime32 RTime32FromRFC3339UTCString( const char* pchValue );
	static const char* RTime32ToRFC3339UTCString( const RTime32 rTime32 );

	// parse time from string "YYYY-MM-DD hh:mm:ss" or just "YYYY-MM-DD", the ' ',':','-' are optional
	static RTime32 RTime32FromString( const char* pszValue );

	// turns RTime32 in a string like "YYYY-MM-DD hh:mm:ss"
	static const char* RTime32ToString( const RTime32 rTime32, bool bNoPunct = false, bool bOnlyDate = false );

	// turns RTime32 into a string like "Aug 21"
	static const char* RTime32ToDayString( const RTime32 rTime32, bool bGMT = false );

	// If the month only has K days, K < N, returns Kth day
	static RTime32 RTime32NthDayOfMonth( const RTime32, int nDay );	// at 00:00:00

	// Add X months but return the Nth day of that month. If the month only has K days, K < N, returns Kth day.
	static RTime32 RTime32MonthAddChooseDay( int nNthDayOfMonth, RTime32 rtime32StartDate, int nMonthsToAdd );

	RTime32 GetBeginningOfDay() const { return RTime32BeginningOfDay( m_nStartTime, m_bGMT ); }		
	RTime32 GetBeginningOfNextDay() const { return RTime32BeginningOfNextDay( m_nStartTime ); }	
	RTime32 GetFirstDayOfMonth() const { return RTime32FirstDayOfMonth( m_nStartTime ); }	
	RTime32 GetLastDayOfMonth() const { return RTime32LastDayOfMonth( m_nStartTime ); }
	RTime32 GetFirstDayOfNextMonth() const { return RTime32FirstDayOfNextMonth( m_nStartTime ); }	
	RTime32 GetLastDayOfNextMonth() const { return RTime32LastDayOfNextMonth( m_nStartTime ); }
	RTime32 GetNthDayOfMonth( int nDay ) const { return RTime32NthDayOfMonth( m_nStartTime, nDay ); }
	RTime32 MonthAddChooseDay( int nNthDayOfMonth, int nMonthsToAdd ) const { return RTime32MonthAddChooseDay( nNthDayOfMonth, m_nStartTime, nMonthsToAdd ); }

	// Add or subtract N days, weeks, minutes, etc from the given time
	static RTime32 RTime32DateAdd( const RTime32, int nAmount, ETimeUnit eTimeAmountType );
	RTime32 DateAdd( int nAmount, ETimeUnit eTimeAmountType ) const { return RTime32DateAdd( m_nStartTime, nAmount, eTimeAmountType ); }

	// Add or subtract a duration from the given time
	static RTime32 RTime32Add( const RTime32, const CRTimeDuration& duration );
	RTime32 Add( const CRTimeDuration& duration ) { return RTime32Add( m_nStartTime, duration ); }

	// Compare two times, and return what the largest time boundary crossed between the two was.
	// Week boundaries do not line up with Month or Year boundaries, so you must rely on pbWeekChanged for them!
	static ETimeUnit FindTimeBoundaryCrossings( RTime32 unTime1, RTime32 unTime2, bool *pbWeekChanged );
	
	void	SetToGMT( bool bUseGMT ) { m_bGMT = bUseGMT;}

private:
	// prevent assignment or copy construction from the server time type
	const CRTime& operator=( const CSTime &val )  { return *this; }
	CRTime( const CSTime& ) {}

	RTime32 m_nStartTime; // the time store by this instance (wall clock, in seconds)
	static RTime32 sm_nTimeCur; // current system wide wall clock time

	static char sm_rgchLocalTimeCur[16]; // string version of time for logging
	static char sm_rgchLocalDateCur[16]; // string version of date for logging
	static RTime32 sm_nTimeLastSystemTimeUpdate; // last time we updated above two logging strings

	bool	m_bGMT;
};

#endif // RTIME_H
