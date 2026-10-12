#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AudioToolbox.h>

static void OSX_MyAudioServicesSystemSoundCompletionProc( SystemSoundID ssID, void *uRLRef )
{
	AudioServicesDisposeSystemSoundID( ssID );
	CFRelease(uRLRef);
}

bool PlaySound( const char *pszFullpath, void *unused1, int unused2 )
{
	SystemSoundID soundID;
	CFStringRef pathstr = CFStringCreateWithCString(NULL, pszFullpath, CFStringGetSystemEncoding());
	if ( !pathstr )
		return false;
	CFURLRef uRLRef = CFURLCreateWithFileSystemPath (kCFAllocatorDefault, pathstr, kCFURLPOSIXPathStyle, FALSE );
	CFRelease(pathstr);

	if ( !uRLRef )
		return false;

	OSStatus err = AudioServicesCreateSystemSoundID( uRLRef, &soundID );
	if ( err != noErr )
	{
		CFRelease(uRLRef);
		return false;
	}

	err = AudioServicesAddSystemSoundCompletion( soundID, NULL, NULL, OSX_MyAudioServicesSystemSoundCompletionProc, (void *)uRLRef );
	if ( err != noErr )
	{
		AudioServicesDisposeSystemSoundID( soundID );
		CFRelease(uRLRef);
		return false;
	}

	AudioServicesPlaySystemSound( soundID );

	return true;
}
