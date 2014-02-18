#include "../q_shared.h"
#include "../cmd.h"
#include "../qcommon.h"
#include "../qcommon_mem.h"

#include <windows.h>
#include <wincrypt.h>
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

void Sys_ShowErrorDialog(const char* functionName);

/*
================
Sys_SetFPUCW
Set FPU control word to default value
================
*/

#ifndef _RC_CHOP
	// mingw doesn't seem to have these defined :(

	#define _MCW_EM 0x0008001fU
	#define _MCW_RC 0x00000300U
	#define _MCW_PC 0x00030000U
	#define _RC_NEAR 0x00000000U
	#define _PC_53 0x00010000U
	  
	unsigned int _controlfp(unsigned int new, unsigned int mask);
#endif

#define FPUCWMASK1 (_MCW_RC | _MCW_EM)
#define FPUCW (_RC_NEAR | _MCW_EM | _PC_53)

#if idx64
	#define FPUCWMASK (FPUCWMASK1)
#else
	#define FPUCWMASK (FPUCWMASK1 | _MCW_PC)
#endif

void Sys_SetFloatEnv(void)
{
	_controlfp(FPUCW, FPUCWMASK);
}

/*
================
Sys_RandomBytes
================
*/
qboolean Sys_RandomBytes( byte *string, int len )
{
	HCRYPTPROV  prov;

	if( !CryptAcquireContext( &prov, NULL, NULL,
		PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) )  {

		return qfalse;
	}

	if( !CryptGenRandom( prov, len, (BYTE *)string ) )  {
		CryptReleaseContext( prov, 0 );
		return qfalse;
	}
	CryptReleaseContext( prov, 0 );
	return qtrue;
}

/*
==================
Sys_Mkdir
==================
*/
qboolean Sys_Mkdir( const char *path )
{

	int result = _mkdir( path );

	if( result != 0 && errno != EEXIST)
		return qfalse;

	return qtrue;
}

/*
==================
Sys_StartProcess

NERVE - SMF
==================
*/
void Sys_StartProcess( char *exeName, qboolean doexit ) {
		TCHAR szPathOrig[_MAX_PATH];
		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory( &si, sizeof( si ) );
		si.cb = sizeof( si );

		GetCurrentDirectory( _MAX_PATH, szPathOrig );

		// JPW NERVE swiped from Sherman's SP code
		if ( !CreateProcess( NULL, va( "%s\\%s", szPathOrig, exeName ), NULL, NULL,FALSE, 0, NULL, NULL, &si, &pi ) ) {
			// couldn't start it, popup error box
			Com_Error( ERR_DROP, "Could not start process: '%s\\%s' ", szPathOrig, exeName );
			return;
		}
		// jpw

		// TTimo: similar way of exiting as used in Sys_OpenURL below
		if ( doexit )
		{
			_exit( 0 );
		}
}

void Sys_DoStartProcess( char *cmdline ) {

	Sys_StartProcess( cmdline, qfalse );
}

void Sys_ReplaceProcess( char *cmdline ) {

	Sys_StartProcess( cmdline, qtrue );
}

qboolean Sys_SetPermissionsExec(const char* ospath)
{
	return qtrue;
}

/*
==================
Sys_SleepSec
==================
*/

void Sys_SleepSec(int seconds)
{
    Sleep(seconds);
}



qboolean Sys_MemoryProtectWrite(void* startoffset, int len)
{

	DWORD oldProtect;

	if(VirtualProtect((LPVOID)startoffset, len, PAGE_READWRITE, &oldProtect) != 0)
	{
	        Sys_ShowErrorDialog("Sys_MemoryProtectWrite");
            return qfalse;
	}

	return qtrue;
}

qboolean Sys_MemoryProtectExec(void* startoffset, int len)
{

	DWORD oldProtect;

	if(VirtualProtect((LPVOID)startoffset, len, PAGE_EXECUTE_READ, &oldProtect) != 0)
	{
            Sys_ShowErrorDialog("Sys_MemoryProtectExec");
            return qfalse;
	}

	return qtrue;
}

qboolean Sys_MemoryProtectReadonly(void* startoffset, int len)
{

	DWORD oldProtect;

	if(VirtualProtect((LPVOID)startoffset, len, PAGE_READONLY, &oldProtect) != 0)
	{
	        Sys_ShowErrorDialog("Sys_MemoryProtectReadonly");
            return qfalse;
	}

	return qtrue;
}

void Sys_ShowErrorDialog(const char* functionName)
{
	void* HWND = NULL;
	char errMessageBuf[1024];
	char displayMessageBuf[1024];
	DWORD lastError = GetLastError();

	if(lastError != 0)
	{
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, lastError, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), errMessageBuf, sizeof(errMessageBuf) -1, NULL);
	}else{
		Q_strncpyz(errMessageBuf, "Unknown Error", sizeof(errMessageBuf));
	}
	
	Com_sprintf(displayMessageBuf, sizeof(displayMessageBuf), "Error in function: %s\nThe error is: %s", functionName, errMessageBuf);
	
	MessageBoxA(HWND, "Errorstring", "System Error in ?", MB_OK | MB_ICONERROR);
}

char *Sys_DefaultHomePath( void ) {
	return NULL;
}



/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

/*
==============
Sys_ListFilteredFiles
==============
*/
void Sys_ListFilteredFiles( const char *basedir, char *subdirs, char *filter, char **list, int *numfiles )
{
	char	search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char	filename[MAX_OSPATH];
	intptr_t	findhandle;
	struct _finddata_t findinfo;

	if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
		return;
	}

	if (strlen(subdirs)) {
		Com_sprintf( search, sizeof(search), "%s\\%s\\*", basedir, subdirs );
	}
	else {
		Com_sprintf( search, sizeof(search), "%s\\*", basedir );
	}

	findhandle = _findfirst (search, &findinfo);
	if (findhandle == -1) {
		return;
	}

	do {
		if (findinfo.attrib & _A_SUBDIR) {
		
			if (Q_stricmp(findinfo.name, ".") && Q_stricmp(findinfo.name, "..")) {
			
				if (strlen(subdirs)) {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s\\%s", subdirs, findinfo.name);
				}
				else {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s", findinfo.name);
				}
				Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
			}
		}
		if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
			break;
		}
		Com_sprintf( filename, sizeof(filename), "%s\\%s", subdirs, findinfo.name );
		if (!Com_FilterPath( filter, filename, qfalse ))
			continue;
		list[ *numfiles ] = CopyString( filename );
		(*numfiles)++;
	} while ( _findnext (findhandle, &findinfo) != -1 );

	_findclose (findhandle);
}

/*
==============
strgtr
==============
*/

static qboolean strgtr(const char *s0, const char *s1)
{
	int l0, l1, i;

	l0 = strlen(s0);
	l1 = strlen(s1);

	if (l1<l0) {
		l0 = l1;
	}

	for(i=0;i<l0;i++) {
		if (s1[i] > s0[i]) {
			return qtrue;
		}
		if (s1[i] < s0[i]) {
			return qfalse;
		}
	}
	return qfalse;
}

/*
==============
Sys_ListFiles
==============
*/
char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs )
{
	char	search[MAX_OSPATH];
	int	nfiles;
	char	**listCopy;
	char	*list[MAX_FOUND_FILES];
	struct _finddata_t findinfo;
	intptr_t	findhandle;
	int	flag;
	int	i;

	if (filter) {

		nfiles = 0;
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

		list[ nfiles ] = 0;
		*numfiles = nfiles;

		if (!nfiles)
		return NULL;

		listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0 ; i < nfiles ; i++ ) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;

		return listCopy;
	}

	if ( !extension) {
		extension = "";
	}

	// passing a slash as extension will find directories
	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		flag = 0;
	} else {
		flag = _A_SUBDIR;
	}

	Com_sprintf( search, sizeof(search), "%s\\*%s", directory, extension );

	// search
	nfiles = 0;

	findhandle = _findfirst (search, &findinfo);
	if (findhandle == -1) {
		*numfiles = 0;
		return NULL;
	}

	do {
		if ( (!wantsubs && flag ^ ( findinfo.attrib & _A_SUBDIR )) || (wantsubs && findinfo.attrib & _A_SUBDIR) ) {
			if ( nfiles == MAX_FOUND_FILES - 1 ) {
				break;
			}
			list[ nfiles ] = CopyString( findinfo.name );
			nfiles++;
		}
	} while ( _findnext (findhandle, &findinfo) != -1 );

	list[ nfiles ] = 0;

	_findclose (findhandle);

	// return a copy of the list
	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	do {
		flag = 0;
		for(i=1; i<nfiles; i++) {
			if (strgtr(listCopy[i-1], listCopy[i])) {
				char *temp = listCopy[i];
				listCopy[i] = listCopy[i-1];
				listCopy[i-1] = temp;
				flag = 1;
			}
		}
	} while(flag);

	return listCopy;
}

/*
==============
Sys_FreeFileList
==============
*/
void Sys_FreeFileList( char **list )
{
	int i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}


qboolean Sys_DirectoryHasContent(const char *dir)
{
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;

    char searchpath[MAX_OSPATH];

	if(strlen(dir) > MAX_OSPATH - 6 || dir[0] == '\0')
		return qfalse;
		
    Q_strncpyz(searchpath, dir, sizeof(searchpath));
	if( searchpath[strlen(searchpath) -1] ==  '\\' )
	{
		searchpath[strlen(searchpath) -1] = '\0';
	}
	Q_strcat(searchpath, sizeof(searchpath), "\\*");

    if((hFind = FindFirstFile(searchpath, &fdFile)) == INVALID_HANDLE_VALUE)
    {
        return qfalse;
    }

    do
    {
        if(stricmp(fdFile.cFileName, ".") != 0 && strcmp(fdFile.cFileName, "..") != 0)
        {
			FindClose(hFind);
			return qtrue;
        }
    }
    while(FindNextFile(hFind, &fdFile));

    FindClose(hFind);

    return qfalse;
}

void Sys_TermProcess( )
{
	Sys_SetFloatEnv();
}

/*
================
Sys_GetCurrentUser
================
*/
const char *Sys_GetUsername( void )
{

	static char s_userName[1024];
	unsigned long size = sizeof( s_userName );

	if( !GetUserName( s_userName, &size ) || !s_userName[0] )
	{
		Q_strncpyz( s_userName, "CoD-Admin", sizeof(s_userName) );
	}

	return s_userName;
}

/*
==============
Sys_Cwd
==============
*/
char *Sys_Cwd( void ) {
	static char cwd[MAX_OSPATH];

	_getcwd( cwd, sizeof( cwd ) - 1 );
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/*
==============
Sys_PlatformInit

Win32 specific initialisation
==============
*/
void Sys_PlatformInit( void )
{

	Sys_SetFloatEnv( );
}


/*
==================
Sys_Backtrace
==================
*/

int Sys_Backtrace(void** buffer, int size)
{
    return 0;
}
