// PbniRegex.cpp : PBNI class
//
// @author : Sebastien Kirche - 2008, 2009, 2011

#define _CRT_SECURE_NO_DEPRECATE
#define _SCL_SECURE_NO_WARNINGS
#define PCRE_STATIC 1

#include <stdio.h>
#include <string>
#ifdef UNICODE
	#include <mbstring.h>
#endif
#include "main.h"
#include "PbniRegex.h"
#include "strconv.h"
#include "pcre.h"
#include "pcrecpp.h"

#ifdef _DEBUG
#define	VERSION_STR	STR(" (Debug version ") STR(PBX_VERSION) STR(" - ") STR(__DATE__) STR(" ") STR(__TIME__) STR(")")
#else
#define	VERSION_STR	STR(" (Release version ") STR(PBX_VERSION) STR(" - ") STR(__DATE__) STR(" ") STR(__TIME__) STR(")")
#endif

char dbgMsg[2048];

PbniRegex::PbniRegex()
{
}

PbniRegex::PbniRegex( IPB_Session * pSession )
:m_pSession( pSession )
{
	OutputDBSIfDebug(STR("PbniRegex :: Constructor\n"));

	m_bGlobal = false;
	m_bCaseSensitive = false;
	m_bmultiLine = false;
	m_bDotNL = false;
	m_bExtended = false;
	m_bUnGreedy = false;
	m_butf8 = true;			//2009-04-02 : always work in utf-8
	m_sPattern = NULL;
	m_sData = NULL;
	re = NULL;
	studinfo = NULL;
	m_matchCount = 0;
	m_maxmatches = MAXMATCHES;
	m_maxgroups = MAXGROUPS;
	m_ovecsize = (m_maxgroups + 1) * 3;
	hHeap = HeapCreate(HEAP_NO_SERIALIZE, 1024 * 64, 0);
	m_matchinfo = (int *)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(int) * (m_maxmatches * m_ovecsize));
	m_replacebuf = (int *)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(int) * m_ovecsize);
	m_groupcount = (int *)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(int) * m_maxmatches);
	m_lastErr = (LPSTR)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, 1);
	m_lastErr[0] = '\0';
	char *test = setlocale(LC_ALL, NULL);
}

PbniRegex::~PbniRegex()
{
	OutputDebugString(STR("PbniRegex :: Destructor\n"));

	if(m_sPattern)
		free(m_sPattern);
	if(m_sData)
		free(m_sData);
	if(re)
		pcre_free(re);
	if(m_matchinfo)
		HeapFree(hHeap, 0, m_matchinfo);
	if(m_replacebuf)
		HeapFree(hHeap, 0, m_replacebuf);
	if(m_groupcount)
		HeapFree(hHeap, 0, m_groupcount);
	if (m_lastErr)
		HeapFree(hHeap, 0, m_lastErr);
	if(hHeap)
		HeapDestroy(hHeap);
}

// method called by PowerBuilder to invoke PBNI class methods
PBXRESULT PbniRegex::Invoke(IPB_Session * session, pbobject obj, pbmethodID mid, PBCallInfo * ci)
{
   PBXRESULT pbxr = PBX_OK;

	switch ( mid ){
		case mid_Version:
			pbxr = this->Version( ci );
			break;
		case mid_Hello:
			pbxr = this->Hello( ci );
			break;
		case mid_Init:
			pbxr = this->Initialize(ci);
			break;
		case mid_Test:
			pbxr = this->Test(ci);
			break;
		case mid_Search:
			pbxr = this->Search(ci);
			break;
		case mid_MatchCount:
			pbxr = this->MatchCount(ci);
			break;
		case mid_MatchPos:
			pbxr = this->MatchPos(ci);
			break;
		case mid_MatchLen:
			pbxr = this->MatchLen(ci);
			break;
		case mid_GroupCount:
			pbxr = this->GroupCount(ci);
			break;
		case mid_Match:
			pbxr = this->Match(ci);
			break;
		case mid_Group:
			pbxr = this->Group(ci);
			break;
		case mid_GroupPos:
			pbxr = this->GroupPos(ci);
			break;
		case mid_GroupLen:
			pbxr = this->GroupLen(ci);
			break;
		case mid_Replace:
			pbxr = this->Replace(ci);
			break;
		case mid_SetMulti:
			pbxr = this->SetMulti(ci);
			break;
		case mid_IsMulti:
			pbxr = this->IsMulti(ci);
			break;
		case mid_IsUtf:
			pbxr = this->IsUtf(ci);
			break;
		case mid_SetUtf:
			pbxr = this->SetUtf(ci);
			break;
		case mid_Study:
			pbxr = this->Study(ci);
			break;
		case mid_getDot:
			pbxr = this->GetDotNL(ci);
			break;
		case mid_setDot:
			pbxr = this->SetDotNL(ci);
			break;
		case mid_getExtended:
			pbxr = this->GetExtended(ci);
			break;
		case mid_setExtended:
			pbxr = this->SetExtended(ci);
			break;
		case mid_getUnGreedy:
			pbxr = this->GetUnGreedy(ci);
			break;
		case mid_setUnGreedy:
			pbxr = this->SetUnGreedy(ci);
			break;
		case mid_getPattern:
			pbxr = this->GetPattern(ci);
			break;
		case mid_getLastErr:
			pbxr = this->GetLastErrMsg(ci);
			break;
#ifdef _DEBUG
		case mid_StrTest:
			pbxr = this->StrTest(ci);
			break;
#endif
		default:
			pbxr = PBX_E_INVOKE_METHOD_AMBIGUOUS;
	}
	return pbxr;
}


void PbniRegex::Destroy() 
{
   delete this;
}

/*====================== Methods exposed to PowerBuilder ==========================*/

PBXRESULT PbniRegex::Version( PBCallInfo * ci )
{
	char verStr[256]; // !!!
	PBXRESULT	pbxr = PBX_OK;

	strcpy(verStr, "PCRE v.");
	strcat(verStr, pcre_version());
#if defined (PBVER) && (PBVER < 100)
	ci->returnValue->SetString(verStr);
#else
	/*int verLen = mbstowcs(NULL, verStr, strlen(verStr)+1);
	LPWSTR wstr = (LPWSTR)malloc((verLen+1) * sizeof(wchar_t));
	mbstowcs(wstr, verStr, strlen(verStr)+1);
	// return value
	ci->returnValue->SetString(wstr);
	free(wstr);*/
	LPCWSTR wver = AnsiStrToWC(verStr);
	ci->returnValue->SetString(wver);
	free((void*)wver);
#endif
	return pbxr;
}

PBXRESULT PbniRegex::Hello( PBCallInfo * ci )
{
	PBXRESULT	pbxr = PBX_OK;

	// return value
	ci->returnValue->SetString( STR("Hello from PbniRegex") VERSION_STR );
	return pbxr;
}

PBXRESULT PbniRegex::Initialize(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;
	//pcre *re;
	const char *error;
	int erroffset;
	int opts = 0;

	if(ci->pArgs->GetCount() != 3)
		return PBX_E_INVOKE_WRONG_NUM_ARGS;

	if(ci->pArgs->GetAt(0)->IsNull())
		ci->returnValue->SetToNull(); //if pattern is null, return null.
	else
	{
		pbstring arg_pattern = ci->pArgs->GetAt(0)->GetString();
		LPCWSTR pattern_ucs2;
#if defined (PBVER) && (PBVER < 100)
		pattern_ucs2 = AnsiStrToWC(m_pSession->GetString(arg_pattern));
#else
		pattern_ucs2 = m_pSession->GetString(arg_pattern);
#endif
		m_bGlobal = ci->pArgs->GetAt(1)->GetBool();
		m_bCaseSensitive = ci->pArgs->GetAt(2)->GetBool();

		if (m_sPattern)
			free(m_sPattern);

		//convert the utf-16 pattern -> utf-8
		int patternLen = WideCharToMultiByte(CP_UTF8,0,pattern_ucs2,-1,NULL,NULL,NULL,NULL);
		m_sPattern = (LPSTR)malloc(patternLen);
		WideCharToMultiByte(CP_UTF8,0,pattern_ucs2,-1,m_sPattern,patternLen,NULL,NULL);	
#if defined (PBVER) && (PBVER < 100)
		free((void*)pattern_ucs2);
#endif
		opts += PCRE_UTF8;

		m_matchCount = 0;
		if(!m_bCaseSensitive)
			opts += PCRE_CASELESS;
		if(m_bmultiLine)
			opts += PCRE_MULTILINE;
		if(m_bDotNL)
			opts += PCRE_DOTALL;
		if(m_bExtended)
			opts += PCRE_EXTENDED;
		if(m_bUnGreedy)
			opts += PCRE_UNGREEDY;

		re = pcre_compile(
				m_sPattern,           /* pattern */
				opts,                 /* options (d�faut = 0)*/
				&error,               /* for error message */
				&erroffset,           /* for error offset */
				NULL);                /* use default character tables */
		if (re == NULL){
			if (error){
				_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PCRE compilation failed at offset %d: %s\n", erroffset, error);
				OutputDebugStringA(dbgMsg);
				SetLastErrMsg(dbgMsg);
			}
			ci->returnValue->SetBool(false);
		}
		else{
			ci->returnValue->SetBool(true);
		}
		if(re){
			int newmaxgroups;
			if (pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &newmaxgroups) == 0){
				if (newmaxgroups > m_maxgroups){
#ifdef _DEBUG
					_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex :: needs to reallocate bigger block for group matches (maxgroup was %d, increase to %d)\n", m_maxgroups, newmaxgroups);
					OutputDebugStringA(dbgMsg);
#endif
					m_maxgroups = newmaxgroups;
					m_ovecsize = (m_maxgroups + 1) * 3;
					m_matchinfo = (int *)HeapReAlloc(hHeap, HEAP_ZERO_MEMORY, m_matchinfo, sizeof(int) * (m_maxmatches * m_ovecsize));
					if(!m_matchinfo){
						long err = GetLastError();
						_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex :: initialize failed on memory reallocation for matches\n");
						OutputDebugStringA(dbgMsg);
						SetLastErrMsg(dbgMsg);
						ci->returnValue->SetBool(false);
						pbxr = PBX_E_OUTOF_MEMORY;
					}
					m_replacebuf = (int *)HeapReAlloc(hHeap, HEAP_ZERO_MEMORY, m_replacebuf, sizeof(int) * m_ovecsize);
					if(!m_replacebuf){
						long err = GetLastError();
						_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex :: initialize failed on memory reallocation for replace\n");
						OutputDebugStringA(dbgMsg);
						SetLastErrMsg(dbgMsg);
						ci->returnValue->SetBool(false);
						pbxr = PBX_E_OUTOF_MEMORY;
					}
					m_groupcount = (int *)HeapReAlloc(hHeap, HEAP_ZERO_MEMORY, m_groupcount, sizeof(int) * m_maxmatches);
					if(!m_groupcount){
						long err = GetLastError();
						_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex :: initialize failed on memory reallocation for groups count\n");
						OutputDebugStringA(dbgMsg);
						SetLastErrMsg(dbgMsg);
						ci->returnValue->SetBool(false);
						pbxr = PBX_E_OUTOF_MEMORY;
					}
				}//mem reallocation needed
			}//pcre-fullinfo was successful
		}//pcre_compile was successful

	}
	return pbxr;
}

/* Analyze the regexp and find out if extra info can be extracted to speed up repetitive calls to it 
   from PCRE documentation (7.3) :
   At present, studying a pattern is useful only for non-anchored patterns that do not have a single fixed starting character.
   A bitmap of possible starting bytes is created. 
*/
PBXRESULT PbniRegex::Study(PBCallInfo *ci)
{
	const char *error;
	PBXRESULT pbxr = PBX_OK;
	
	studinfo = pcre_study(re, 0, &error);
	if (error){
	  _snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PCRE study failed with message '%s'\n", error);
	  OutputDebugStringA(dbgMsg);
	  SetLastErrMsg(dbgMsg);
	}
	if (studinfo == NULL){
		//if the study did not found useful data
		ci->returnValue->SetBool(false);
	}else{
		//if the study was successful
		ci->returnValue->SetBool(true);
	}
	return pbxr;
}

/* Tell is the given string matches the regexp pattern */
PBXRESULT PbniRegex::Test( PBCallInfo * ci )
{
	PBXRESULT	pbxr = PBX_OK;
	int rc;

	// check arguments
	if ( ci->pArgs->GetAt(0)->IsNull() || !re){
		// if any of the passed arguments is null, return the null value
		ci->returnValue->SetBool(false);
	}
	else{
		pbstring pbtest = ci->pArgs->GetAt(0)->GetString();
		LPCWSTR testStr;
#if defined (PBVER) && (PBVER < 100)
		testStr = AnsiStrToWC(m_pSession->GetString(pbtest));
#else
		testStr = m_pSession->GetString(pbtest);
#endif

		//convert the test string -> utf-8
		int testLen = WideCharToMultiByte(CP_UTF8,0,testStr,-1,NULL,0,NULL,NULL);
		LPSTR testStr_utf8 = (LPSTR)malloc(testLen);
		WideCharToMultiByte(CP_UTF8,0,testStr,-1,testStr_utf8,testLen,NULL,NULL);	

		rc = pcre_exec(
		  re,                   /* the compiled pattern */
		  studinfo,             /* extra data if we studied the pattern */
		  testStr_utf8,         /* the subject string */
		  testLen-1,			/* the length of the subject */
		  0,                    /* start at offset 0 in the subject */
		  0,                    /* default options */
		  //TODO the m_matchinfo can be too short for all matching info
		  m_matchinfo,          /* output vector for substring information */
		  m_ovecsize);			/* number of elements in the output vector */
		if(rc < 0){
			switch(rc){
				case PCRE_ERROR_NOMATCH:
					//normal : no match
					break;
				default:
					//other error case
					break;
			}
			ci->returnValue->SetBool(false);
		}
		else
			ci->returnValue->SetBool(true);

		free(testStr_utf8);
#if defined (PBVER) && (PBVER < 100)
		free((void*)testStr);
#endif
	}
	return pbxr;
}

PBXRESULT PbniRegex::SetUtf(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	//2009-04-02 : always work with utf-8 
	//m_butf8 = ci->pArgs->GetAt(0)->GetBool();
	return pbxr;
}

PBXRESULT PbniRegex::SetDotNL(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	m_bDotNL = ci->pArgs->GetAt(0)->GetBool();
	return pbxr;
}

PBXRESULT PbniRegex::SetExtended(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	m_bExtended = ci->pArgs->GetAt(0)->GetBool();
	return pbxr;
}

PBXRESULT PbniRegex::SetMulti(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	m_bmultiLine = ci->pArgs->GetAt(0)->GetBool();
	return pbxr;
}

PBXRESULT PbniRegex::SetUnGreedy(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	m_bUnGreedy = ci->pArgs->GetAt(0)->GetBool();
	return pbxr;
}

PBXRESULT PbniRegex::Search(PBCallInfo *ci)
{
	int nmatch = 0;
	int startoffset = 0;
	int res;
	PBXRESULT pbxr = PBX_OK;

	if(ci->pArgs->GetAt(0)->IsNull()){
		// if any of the passed arguments is null, return the null value
		ci->returnValue->SetToNull();
	}
	else {
		pbstring pbsearch = ci->pArgs->GetAt(0)->GetString();
		LPCWSTR searchStr;

		if(m_sData)
			free(m_sData);

#if defined (PBVER) && (PBVER < 100)
		searchStr = AnsiStrToWC(m_pSession->GetString(pbsearch));
#else
		searchStr = m_pSession->GetString(pbsearch);
#endif

		//convert searched string -> utf-8
		int searchLen = WideCharToMultiByte(CP_UTF8,0,searchStr,-1,NULL,0,NULL,NULL);
		m_sData = (LPSTR)malloc(searchLen);
		WideCharToMultiByte(CP_UTF8,0,searchStr,-1,m_sData,searchLen,NULL,NULL);	
#if defined (PBVER) && (PBVER < 100)
		free((void *)searchStr);
#endif

		do {
			if (nmatch >= m_maxmatches){
				//if we have already filled the vector to the maximum number of matches, reallocate some more space
#ifdef _DEBUG
				_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex :: needs more memory to store matches (max matches was %d)\n", m_maxmatches);
				OutputDebugStringA(dbgMsg);
#endif
				//lets say that we grow the buffer by 50%
				m_maxmatches += m_maxmatches >> 2;
				//m_maxmatches *= 1.5;
				m_maxmatches++; //keep ?
#ifdef _DEBUG
				_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex :: new max matches will be %d\n", m_maxmatches);
				OutputDebugStringA(dbgMsg);
#endif
				//TODO: should handle a try/catch if the mem alloc fails ?
				m_matchinfo = (int *)HeapReAlloc(hHeap, HEAP_ZERO_MEMORY, m_matchinfo, sizeof(int) * (m_maxmatches * m_ovecsize));
				if(!m_matchinfo){
					long err = GetLastError();
					_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex :: search failed on memory reallocation for matches\n");
					OutputDebugStringA(dbgMsg);
					SetLastErrMsg(dbgMsg);
					ci->returnValue->SetLong(-1);
					pbxr = PBX_E_OUTOF_MEMORY;
				}
				m_groupcount = (int *)HeapReAlloc(hHeap, HEAP_ZERO_MEMORY, m_groupcount, sizeof(int) * m_maxmatches);
				if(!m_groupcount){
					long err = GetLastError();
					_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex :: initialize failed on memory reallocation for groups count\n");
					OutputDebugStringA(dbgMsg);
					SetLastErrMsg(dbgMsg);
					ci->returnValue->SetLong(-1);
					pbxr = PBX_E_OUTOF_MEMORY;
				}
			}
			res = pcre_exec(re, studinfo, m_sData, (int)searchLen-1, startoffset, 0, &(m_matchinfo[nmatch * m_ovecsize]), m_ovecsize);
			if (res > 0) {
				m_groupcount[nmatch] = res - 1;
				startoffset = m_matchinfo[(nmatch * m_ovecsize) + 1];
				nmatch++;
				// the match might have a null length : the start offset = the next offset, so we stop there
				if(startoffset == m_matchinfo[((nmatch - 1) * m_ovecsize)])
					break;
				// do not perform another search if we are at the end of the string
				if (startoffset >= (searchLen - 1))
					break;
			} else
				break;
				//res = 0 if not enough space in the vector for all groups - this should not happen

		//until there is no match left OR if we did not set the global attribute for a single search
		}while (m_bGlobal && nmatch > 0); //negative res for errors or no match
		
		m_matchCount = nmatch;
		if (nmatch)
			ci->returnValue->SetLong(nmatch);
		else
			ci->returnValue->SetLong(-1);
	}

	return pbxr;
}

PBXRESULT PbniRegex::MatchCount(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	ci->returnValue->SetLong(m_matchCount);
	return pbxr;
}

PBXRESULT PbniRegex::IsMulti(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	ci->returnValue->SetBool(m_bmultiLine);
	return pbxr;
}

PBXRESULT PbniRegex::IsUtf(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	ci->returnValue->SetBool(m_butf8);
	return pbxr;
}

PBXRESULT PbniRegex::GetDotNL(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	ci->returnValue->SetBool(m_bDotNL);
	return pbxr;
}

PBXRESULT PbniRegex::GetExtended(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	ci->returnValue->SetBool(m_bExtended);
	return pbxr;
}

PBXRESULT PbniRegex::GetUnGreedy(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	ci->returnValue->SetBool(m_bUnGreedy);
	return pbxr;
}

//preliminary versions for an utf-8 compliant strlen()
int strlen_utf8(const unsigned char *ptr){
	int c = 0;
	while(*ptr){
		if((*ptr & 0xC0)!=0x80){
			c++;
		}
		ptr++;		
	}
	return c;
}

//utf-8 compliant strlen that can parse a limited amount of bytes
int strnlen_utf8(const unsigned char* ptr, unsigned int maxbytes){
	int c = 0;	
	while(*ptr && maxbytes){
		if((*ptr & 0xC0)!=0x80){
			//printf("\nDEBUG c++ : %c ; %d ; %d ; MASKED = %02X", *ptr, c, maxbytes, (*ptr & 0xC0));
			c++;
		}
		ptr++;		
		maxbytes--;
		//printf("\nDEBUG ptr++ : %c ; %d ; %d", *ptr, c, maxbytes);
	}
	return c;
}

PBXRESULT PbniRegex::MatchPos(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	long index = ci->pArgs->GetAt(0)->GetLong() - 1; //in PB the index starts at 1
	if(index >= 0 && index < m_matchCount)
		ci->returnValue->SetLong(strnlen_utf8((const unsigned char*)m_sData, m_matchinfo[index * m_ovecsize + 0]) + 1);
	else
		ci->returnValue->SetLong(-1);
	return pbxr;
}

PBXRESULT PbniRegex::MatchLen(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	long index = ci->pArgs->GetAt(0)->GetLong() - 1; //in PB the index starts at 1
	if(index >= 0 && index <= m_matchCount)
		ci->returnValue->SetLong(strnlen_utf8((const unsigned char*)m_sData + m_matchinfo[index * m_ovecsize + 0], m_matchinfo[index * m_ovecsize + 1] - m_matchinfo[index * m_ovecsize + 0]));
	else
		ci->returnValue->SetLong(-1);
	return pbxr;
}

PBXRESULT PbniRegex::GroupPos(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	if(ci->pArgs->GetCount() != 2)
		return PBX_E_INVOKE_WRONG_NUM_ARGS;

	long matchindex = ci->pArgs->GetAt(0)->GetLong() - 1; // for PB, first match is 1

	if(matchindex >= 0 && matchindex <= m_matchCount){
		long groupindex = ci->pArgs->GetAt(1)->GetLong();//group 0 is the whole match
		if(groupindex >= 0 && groupindex <= m_groupcount[matchindex])
			ci->returnValue->SetLong(strnlen_utf8((const unsigned char*)m_sData, m_matchinfo[matchindex * m_ovecsize + 2 * groupindex]) + 1);
		else
			ci->returnValue->SetLong(-1);
	}
	else
		ci->returnValue->SetLong(-1);
	return pbxr;
}

PBXRESULT PbniRegex::GroupLen(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	if(ci->pArgs->GetCount() != 2)
		return PBX_E_INVOKE_WRONG_NUM_ARGS;

	long matchindex = ci->pArgs->GetAt(0)->GetLong() - 1; // for PB, first match is 1

	if(matchindex >= 0 && matchindex <= m_matchCount){
		long groupindex = ci->pArgs->GetAt(1)->GetLong();//group 0 is the whole match
		if(groupindex >= 0 && groupindex <= m_groupcount[matchindex])
			ci->returnValue->SetLong(strnlen_utf8((const unsigned char*)m_sData + m_matchinfo[matchindex * m_ovecsize + 2 * groupindex], m_matchinfo[matchindex * m_ovecsize + 2 * groupindex + 1] - m_matchinfo[matchindex * m_ovecsize + 2 * groupindex]));
		else
			ci->returnValue->SetLong(-1);
	}
	else
		ci->returnValue->SetLong(-1);
	return pbxr;
}

PBXRESULT PbniRegex::GroupCount(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	long index = ci->pArgs->GetAt(0)->GetLong() - 1; //in PB the index starts at 1

	if(index >= 0 && index <= m_matchCount)
			ci->returnValue->SetLong(m_groupcount[index]);
	else
		ci->returnValue->SetLong(-1);
	return pbxr;
}

PBXRESULT PbniRegex::Match(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	int matchLen, matchLenW;
	long index = ci->pArgs->GetAt(0)->GetLong() - 1; //in PB the index starts at 1

	if(index >= 0 && index <= m_matchCount){
		//extract the match from the data
		matchLen = m_matchinfo[index * m_ovecsize + 1] - m_matchinfo[index * m_ovecsize + 0] + 1;
		LPSTR match = (LPSTR)malloc(matchLen + 1);
		lstrcpynA(match, (LPCSTR)(m_sData + m_matchinfo[index * m_ovecsize + 0]), matchLen);
		//convert in WC
		matchLenW = MultiByteToWideChar(CP_UTF8, 0, match, -1, NULL, 0);
		LPWSTR wstr = (LPWSTR)malloc((matchLenW) * sizeof(wchar_t));
		MultiByteToWideChar(CP_UTF8, 0, match, -1, wstr, matchLenW);

		// return value
#if defined (PBVER) && (PBVER < 100)
		LPCSTR ansiRet = WCToAnsiStr(wstr);
		ci->returnValue->SetString(ansiRet);
		free((void*)ansiRet);
#else
		ci->returnValue->SetString(wstr);
#endif
		free(wstr);
		free(match);
	}
	else
		ci->returnValue->SetToNull();
	return pbxr;
}

PBXRESULT PbniRegex::GetPattern(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;
	
	if (m_sPattern) {
		int lenW = MultiByteToWideChar(CP_UTF8, 0, m_sPattern, -1, NULL, 0);
		LPWSTR wstr = (LPWSTR)malloc((lenW) * sizeof(wchar_t));
		MultiByteToWideChar(CP_UTF8, 0, m_sPattern, -1, wstr, lenW);

#if defined (PBVER) && (PBVER < 100)
		LPCSTR ansiStr = WCToAnsiStr(wstr);
		ci->returnValue->SetString(ansiStr);
		free((void*)ansiStr);
#else
		ci->returnValue->SetString(wstr);
#endif
		free(wstr);
	} 
	else
		ci->returnValue->SetToNull();

	return pbxr;
}

PBXRESULT PbniRegex::GetLastErrMsg(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	if (m_lastErr){
		int errLen = mbstowcs(NULL, m_lastErr, strlen(m_lastErr)+1);
		LPWSTR werr = (LPWSTR)malloc((errLen+1) * sizeof(wchar_t));
		mbstowcs(werr, m_lastErr, strlen(m_lastErr)+1);

		ci->returnValue->SetString((LPCTSTR)werr);
		free(werr);
	}
	else
		ci->returnValue->SetString(STR(""));

	return pbxr;
}

void PbniRegex::SetLastErrMsg(const char *msg)
{
	m_lastErr = (LPSTR)HeapReAlloc(hHeap, HEAP_ZERO_MEMORY, m_lastErr, strlen(msg) + 1);
	if (m_lastErr)
		strcpy(m_lastErr, msg);
}

PBXRESULT PbniRegex::Group(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;
	int groupLen, groupLenW;

	if(ci->pArgs->GetCount() != 2)
		return PBX_E_INVOKE_WRONG_NUM_ARGS;

	long matchindex = ci->pArgs->GetAt(0)->GetLong() - 1; //in PB the index starts at 1

	if(matchindex >= 0 && matchindex <= m_matchCount)
	{
		long groupindex = ci->pArgs->GetAt(1)->GetLong(); //the group 0 is the whole match

		//TODO: replace with pcre_copy_substring() or pcre_get_substring()
		
		if(groupindex >= 0 && groupindex <= m_groupcount[matchindex]){
			if(-1 == m_matchinfo[matchindex * m_ovecsize + 2*groupindex]){
				//the group matched nothing
				ci->returnValue->SetToNull();
			}
			else {
				//extract the match from the data
				groupLen = m_matchinfo[matchindex * m_ovecsize + 2*groupindex + 1] - m_matchinfo[matchindex * m_ovecsize + 2*groupindex] + 1;
				LPSTR group = (LPSTR)malloc(groupLen + 1);
				lstrcpynA(group, (LPCSTR)(m_sData + m_matchinfo[matchindex * m_ovecsize + 2*groupindex]), groupLen);
				//convert in WC
				groupLenW = MultiByteToWideChar(CP_UTF8, 0, group, -1, NULL, 0);
				LPWSTR wstr = (LPWSTR)malloc((groupLenW) * sizeof(wchar_t));
				MultiByteToWideChar(CP_UTF8, 0, group, -1, wstr, groupLenW);

				// return value
#if defined (PBVER) && (PBVER < 100)
				LPCSTR ansiRet = WCToAnsiStr(wstr);
				ci->returnValue->SetString(ansiRet);
				free((void*)ansiRet);
#else
				ci->returnValue->SetString(wstr);
#endif
				free(wstr);
				free(group);
			}
		}
		else
			ci->returnValue->SetToNull();	//groupindex < 0 or groupindex > groupcount
	}
	else
		ci->returnValue->SetToNull(); //matchindex < 0 or matchindex > matchcount
	return pbxr;
}

#ifdef _DEBUG
char EURO_CP1252[] = {0x80};
char EURO_UTF8[] = {0xE2, 0x82, 0xAC};
char TEST[] = {0x80, 0xE9};

PBXRESULT PbniRegex::StrTest( PBCallInfo * ci )
{
	PBXRESULT	pbxr = PBX_OK;

	if(ci->pArgs->GetCount() != 1)
		return PBX_E_INVOKE_WRONG_NUM_ARGS;

	pbstring pbstr = ci->pArgs->GetAt(0)->GetString();
	LPCTSTR strIn = m_pSession->GetString(pbstr);
	TCHAR strTest[] = STR("the input string was :");
	LPTCH buf;

	if (_tcslen(strIn)){
		buf = new TCHAR[_tcslen(strTest) + _tcslen(strIn) + 1];
		_tcscpy(buf, strTest);
		_tcscat(buf, strIn);
	}else{

#if defined (PBVER) && (PBVER < 100)
		buf = TEST;
#else
		int bufLen;
		bufLen = _tcslen(strTest) + MultiByteToWideChar(CP_ACP,0,TEST,sizeof(TEST),0,0) + 1;
		buf = new TCHAR[wcslen(strTest)+sizeof(TEST)+1];
		memset(buf, 0, bufLen*sizeof(TCHAR));
		LPCWSTR valeur = AnsiStrToWC(TEST);
		wcscpy(buf, strTest);
		wcscat(buf,valeur);
		free((void*)valeur);
#endif
		

	}

	// return value
	ci->returnValue->SetString( buf );
#if defined (PBVER) && (PBVER >= 100)
	delete(buf);
#endif

	return pbxr;
}
#endif

PBXRESULT PbniRegex::Replace(PBCallInfo *ci)
{
	int nmatch = 0;
	int nbgroups;
	int startoffset = 0;
	int res, matchLen = 0;
	char toexp[10];
	PBXRESULT pbxr = PBX_OK;

	if(ci->pArgs->GetCount() != 2)
		return PBX_E_INVOKE_WRONG_NUM_ARGS;
	
	if(ci->pArgs->GetAt(0)->IsNull() || ci->pArgs->GetAt(1)->IsNull())
		ci->returnValue->SetToNull();
	else {
		pbstring pbsearch = ci->pArgs->GetAt(0)->GetString();
		LPCWSTR searchWStr;

		//get the utf-16 string
#if defined (PBVER) && (PBVER < 100)
		searchWStr = AnsiStrToWC(m_pSession->GetString(pbsearch));
#else
		searchWStr = m_pSession->GetString(pbsearch);
#endif
		//search string utf-16 -> utf-8
		int searchLen = WideCharToMultiByte(CP_UTF8,0,searchWStr,-1,NULL,0,NULL,NULL);
		LPSTR search_utf8 = (LPSTR)malloc(searchLen);
		WideCharToMultiByte(CP_UTF8,0,searchWStr,-1,search_utf8,searchLen,NULL,NULL);
#if defined (PBVER) && (PBVER < 100)
		free((void*)searchWStr);
#endif
		//replace string utf-16 -> utf-8
		pbstring pbreplace = ci->pArgs->GetAt(1)->GetString();
		LPCWSTR replaceWStr;
#if defined (PBVER) && (PBVER < 100)
		replaceWStr = AnsiStrToWC(m_pSession->GetString(pbreplace));
#else
		replaceWStr = m_pSession->GetString(pbreplace);
#endif
		int repLen = WideCharToMultiByte(CP_UTF8,0,replaceWStr,-1,NULL,0,NULL,NULL);
		LPSTR rep_utf8 = (LPSTR)malloc(repLen);
		WideCharToMultiByte(CP_UTF8,0,replaceWStr,-1,rep_utf8,repLen,NULL,NULL);
#if defined (PBVER) && (PBVER < 100)
		free((void*)replaceWStr);
#endif

		using namespace std;
		string working (search_utf8);

		//get the number of capturing patterns
		res = pcre_fullinfo(re, studinfo, PCRE_INFO_CAPTURECOUNT, &nbgroups); //need to check res ?
		do {
			//crawl the matches
			res = pcre_exec(re, studinfo, working.c_str(), strlen(working.c_str()), startoffset, 0, m_replacebuf, m_ovecsize);
			if (res > 0) {
				string rep(rep_utf8);
				unsigned int p=0, k, grplen, bck;
				
				//expansion of substrings
				for (int j = nbgroups; j >= 0; j--){
					_snprintf(toexp, sizeof(toexp) - 1, "\\%d", j);
					grplen = strlen(toexp);
					p = 0;
					while ((p = rep.find(toexp, p)) != string::npos){
						//check if escaped backslashes
						bck= p ? 0 : 1;
						for (k=p; k > 0 && rep[k]=='\\'; k--){
							bck++;
						}
						if (bck & 1){ //if odd number of backslashes it's ok to replace
							if (m_replacebuf[j*2] > -1) //when a group matches nothing its offset equals -1
								//do NOT map byte offsets into characters offsets : it is ok for replacement
								rep.replace(p, grplen, working.substr(m_replacebuf[j*2], m_replacebuf[j*2 +1] - m_replacebuf[j*2]));
							else
								rep.erase(p, grplen);
						} else
							p += grplen;
					}
				}
				//replace escaped backslashes
				p=0;
				while ((p = rep.find("\\\\", p)) != string::npos){
					rep.replace(p++, 2, "\\"); //replace and increase pointer, for not eating all successive backslashes
				}

				//replace the match by the replace string
				matchLen = m_replacebuf[1] - m_replacebuf[0];
				repLen = rep.length();
				working.replace(m_replacebuf[0], matchLen, rep);
				startoffset = m_replacebuf[0] + repLen;
				nmatch++;
			}
		//until there is no match left OR if we did not set the global attribute for a single replacement
		//also, if we replaced nothing by nothing, we can stop too
		}while (m_bGlobal && (res > 0) && (matchLen || repLen));

#ifdef _DEBUG
		OutputDebugStringA(working.c_str());
#endif
		//result string  utf-8 -> utf-16
		int outLen = MultiByteToWideChar(CP_UTF8, 0, working.c_str(), -1, NULL, 0);
		LPWSTR wstr = (LPWSTR)malloc(outLen * sizeof(wchar_t));
		MultiByteToWideChar(CP_UTF8, 0, working.c_str(), -1, wstr, outLen);

#if defined (PBVER) && (PBVER < 100)
		LPCSTR ansiStr = WCToAnsiStr(wstr);
		ci->returnValue->SetString(ansiStr);
		free((void*)ansiStr);
#else
		ci->returnValue->SetString(wstr);
#endif

		free(search_utf8);
		free(rep_utf8);
		free(wstr);
	}
	return pbxr;
}

PBXRESULT PbniRegex::FastReplace(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	if(ci->pArgs->GetCount() != 3)
		//if parameter count  <> 3 -> error to PB
		return PBX_E_INVOKE_WRONG_NUM_ARGS;
	if(ci->pArgs->GetAt(0)->IsNull() || ci->pArgs->GetAt(1)->IsNull() || ci->pArgs->GetAt(2)->IsNull())
		//if at less 1 param is null, return null
		ci->returnValue->SetToNull();
	else{
		using namespace std; //for std::wstring
		pbstring source = ci->pArgs->GetAt(0)->GetString();
		pbstring pattern = ci->pArgs->GetAt(1)->GetString();

		LPCTSTR s = m_pSession->GetString(source);
		LPCTSTR p = m_pSession->GetString(pattern);
#ifdef _DEBUG
		_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex::FastReplace, source = %ls\n", s);
		OutputDebugStringA(dbgMsg);
		_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex::FastReplace, pattern = %ls\n", p);
		OutputDebugStringA(dbgMsg);
#endif
		wchar_t* pos = wcsstr((wchar_t*)s, (wchar_t*)p);
		if(0){
			stdstring sourcew(s);
			stdstring patternw(p);
		}
		//test for one occurence
		if(/*_tcsstr(s, p)*/pos){
		
			OutputDBSIfDebugA("PbniRegex::FastReplace, found at least one occurrence.\n");
			pbstring replace = ci->pArgs->GetAt(2)->GetString();
			//stdstring replacew(m_pSession->GetString(replace));
			wchar_t* rpl = (wchar_t*)m_pSession->GetString(replace);
			wchar_t* trg = wcsdup(s);
			wchar_t* prev;
			//here is the 'all' of 'replaceall' : replace each occurence
			//unsigned int p = 0, startoffset = 0;
			size_t p_len = wcslen(p);
			size_t r_len = wcslen(rpl);

			if(p_len<r_len){
			//si length(rpl) > length(pattern) alors il faut compter combien il y a d'occurance
			// et augmenter le buffer de la chaine de destination de: Occurences x ( length(rpl) - length(pattern) )
				prev=pos;
				unsigned long occ = 1;
				while(pos=wcsstr(pos + p_len, p)){
					occ++;
				}
				pos = prev;
				trg = (wchar_t*)realloc((void*)trg, sizeof(wchar_t)*(1 + wcslen(s) + occ*(r_len - p_len) ) );
			}
			prev = (wchar_t*)s;
			trg[0] = (wchar_t)0;
			wchar_t* tmp_trg = trg;
			size_t t_len;		//taille des chunks interm�diaires
			while(pos/*(p = sourcew.find(patternw, startoffset)) != string::npos*/){
#ifdef _DEBUG
//				_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex::FastReplace, sourcew.find(%ls, %d) found offset %d\n", patternw.c_str(), startoffset, p);
//				OutputDebugStringA(dbgMsg);
#endif
				//sourcew.replace(p, patternw.length(), replacew);
				//startoffset = p + replacew.length();
				
				t_len = pos - prev;
				wcsncat(tmp_trg, prev, t_len);	//copie ce qui est avant le match (depuis le pr�c�dent match || d�but)
				tmp_trg += t_len;
				wcscat(tmp_trg, rpl);
				tmp_trg += r_len;
				prev = pos + p_len;
				pos = wcsstr(prev,p);
			}
			wcscat(tmp_trg, prev);
			//return the resulting string
#ifdef _DEBUG
//			_snprintf(dbgMsg, sizeof(dbgMsg) - 1, "PbniRegex::FastReplace, final string is %ls\n", sourcew.c_str());
//			OutputDebugStringA(dbgMsg);
#endif
			ci->returnValue->SetString(trg/*sourcew.c_str()*/);
			//free(trg);//do not need anymore, but crash immediatelly in Debug and sometime on release
		}
		else {
			//if no occurrence, return the given string
			OutputDBSIfDebugA("PbniRegex::FastReplace, no occurrence.\n");
			ci->returnValue->SetPBString(source);
		}
	}
	return pbxr;
}

PBXRESULT PbniRegex::FastReplaceCase(PBCallInfo *ci)
{
	using namespace std; //for std::wstring
	pbstring source = ci->pArgs->GetAt(0)->GetString();
	pbstring pattern = ci->pArgs->GetAt(1)->GetString();
	pbstring replace = ci->pArgs->GetAt(2)->GetString();

	stdstring sourcew(m_pSession->GetString(source));
	stdstring patternw(m_pSession->GetString(pattern));
	stdstring replacew(m_pSession->GetString(replace));

	unsigned int p = 0, startoffset = 0;
	//test for one occurence
	if((p = sourcew.find(patternw, startoffset)) != string::npos){
		//here is the 'all' of 'replaceall' : replace each occurence
		do{	
			sourcew.replace(p, patternw.length(), replacew);
			startoffset = p + replacew.length();
		} while((p = sourcew.find(patternw, startoffset)) != string::npos);
		//return the resulting string
		ci->returnValue->SetString(sourcew.c_str());
	}
	else
		//if no occurrence, return the given string
		ci->returnValue->SetPBString(source);

	return PBX_OK;
}

// case insensitive character traits
// inherited copy (preserves case),
// case insensitive comparison, search
struct traits_nocase : std::char_traits<char>
{
	static bool eq( const char& c1, const char& c2 ) { return toupper(c1) == toupper(c2) ; }
	static bool lt( const char& c1, const char& c2 ) { return toupper(c1) < toupper(c2) ; }
	static int compare( const char* s1, const char* s2, size_t N )
	{
	  //return strncasecmp( s1, s2, N ) ; // posix
	  return _strnicmp( s1, s2, N ) ; //microsoft
	}
	static const char* find( const char* s, size_t N, const char& a )
	{
	  for( size_t i=0 ; i<N ; ++i )
		if( toupper(s[i]) == toupper(a) ) return s+i ;
	  return 0 ;
	}
	static bool eq_int_type ( const int_type& c1, const int_type& c2 ) { return toupper(c1) == toupper(c2) ; }
};
// string preserves case; comparisons are case insensitive
typedef std::basic_string< char, traits_nocase > string_nocase ;

// make string_nocase work like a std::string
//           with streams using std::char_traits
// std::basic_istream< char, std::char_traits<char> > (std::istream) and
// std::basic_ostream< char, std::char_traits<char> > (std::ostream)
inline std::ostream& operator<< ( std::ostream& stm, const string_nocase& str )
{
	return stm << reinterpret_cast<const std::string&>(str); 
}

inline std::istream& operator>> ( std::istream& stm, string_nocase& str )
{
	std::string s ; stm >> s ;
	if(stm) str.assign(s.begin(),s.end()) ;
	return stm ;
}

inline std::istream& getline( std::istream& stm, string_nocase& str )
{
	std::string s ; std::getline(stm,s) ;
	if(stm) str.assign(s.begin(),s.end()) ;
	return stm ;
}

// case insensitive character traits
// inherited copy (preserves case),
// case insensitive comparison, search
struct traitws_nocase : std::char_traits<TCHAR>
{
	static bool eq( const TCHAR& c1, const TCHAR& c2 ) { return toupper(c1) == toupper(c2) ; }
	static bool lt( const TCHAR& c1, const TCHAR& c2 ) { return toupper(c1) < toupper(c2) ; }
	static int compare( const TCHAR* s1, const TCHAR* s2, size_t N )
	{
	  //return strncasecmp( s1, s2, N ) ; // posix
	  return _tcsnicmp( s1, s2, N ) ; //microsoft
	}
	static const TCHAR* find( const TCHAR* s, size_t N, const TCHAR& a )
	{
	  for( size_t i=0 ; i<N ; ++i )
		if( toupper(s[i]) == toupper(a) ) return s+i ;
	  return 0 ;
	}
	static bool eq_int_type ( const int_type& c1, const int_type& c2 ) { return toupper(c1) == toupper(c2) ; }
};
// string preserves case; comparisons are case insensitive
typedef std::basic_string< TCHAR, traitws_nocase > tstring_nocase ;

// make wstring_nocase work like a std::wstring
//           with streams using std::char_traits
// std::basic_istream< char, std::char_traits<char> > (std::istream) and
// std::basic_ostream< char, std::char_traits<char> > (std::ostream)
#if defined (PBVER) && (PBVER < 100)
inline std::ostream& operator<< ( std::ostream& stm, const tstring_nocase& str )
{
	return stm << reinterpret_cast<const std::string&>(str); 
}

inline std::istream& operator>> ( std::istream& stm, tstring_nocase& str )
{
	std::string s ; stm >> s ;
	if(stm) str.assign(s.begin(),s.end()) ;
	return stm ;
}

inline std::istream& getline( std::istream& stm, tstring_nocase& str )
{
	std::string s ; std::getline(stm,s) ;
	if(stm) str.assign(s.begin(),s.end()) ;
	return stm ;
}

#else
inline std::wostream& operator<< ( std::wostream& stm, const tstring_nocase& str )
{
	return stm << reinterpret_cast<const std::wstring&>(str); 
}

inline std::wistream& operator>> ( std::wistream& stm, tstring_nocase& str )
{
	std::wstring s ; stm >> s ;
	if(stm) str.assign(s.begin(),s.end()) ;
	return stm ;
}

inline std::wistream& getline( std::wistream& stm, tstring_nocase& str )
{
	std::wstring s ; std::getline(stm,s) ;
	if(stm) str.assign(s.begin(),s.end()) ;
	return stm ;
}
#endif

PBXRESULT PbniRegex::FastReplaceNoCase(PBCallInfo *ci)
{
	using namespace std; //for std::wstring
	pbstring source = ci->pArgs->GetAt(0)->GetString();
	pbstring pattern = ci->pArgs->GetAt(1)->GetString();
	pbstring replace = ci->pArgs->GetAt(2)->GetString();

	tstring_nocase sourcew(m_pSession->GetString(source));
	tstring_nocase patternw(m_pSession->GetString(pattern));
	tstring_nocase replacew(m_pSession->GetString(replace));

	unsigned int p = 0, startoffset = 0;
	//test for one occurence
	if((p = sourcew.find(patternw, startoffset)) != string_nocase::npos){
		//here is the 'all' of 'replaceall' : replace each occurence
		do{	
			sourcew.replace(p, patternw.length(), replacew);
			startoffset = p + replacew.length();
		} while((p = sourcew.find(patternw, startoffset)) != string_nocase::npos);
		//return the resulting string
		ci->returnValue->SetString(sourcew.c_str());
	}
	else
		//if no occurrence, return the given string
		ci->returnValue->SetPBString(source);

	return PBX_OK;
}

PBXRESULT PbniRegex::FastReplaceChooseCase(PBCallInfo *ci)
{
	PBXRESULT pbxr = PBX_OK;

	if(ci->pArgs->GetCount() != 4)
		//if parameter count  <> 4 -> error to PB
		return PBX_E_INVOKE_WRONG_NUM_ARGS;
	if(ci->pArgs->GetAt(0)->IsNull() || ci->pArgs->GetAt(1)->IsNull() || ci->pArgs->GetAt(2)->IsNull())
		//if at less 1 param is null, return null
		ci->returnValue->SetToNull();
	else{
		using namespace std; //for std::wstring
		pbboolean casesensitive = FALSE;

		casesensitive = ci->pArgs->GetAt(3)->GetBool();
		if(casesensitive)
			pbxr = FastReplaceCase(ci);
		else
			pbxr = FastReplaceNoCase(ci);
	}
	return pbxr;
}

