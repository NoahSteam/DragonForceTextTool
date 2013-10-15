#if (_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400)
#define WM_MOUSEWHEEL                   0x020A
#endif

#ifndef WINVER                // Allow use of features specific to Windows 95 and Windows NT 4 or later.
#define WINVER 0x0501        // Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif

#ifndef _WIN32_WINNT        // Allow use of features specific to Windows NT 4 or later.
#define _WIN32_WINNT 0x0501        // Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif                        

#ifndef _WIN32_WINDOWS        // Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0501 // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef WINDOWS_LEAN_AND_MEAN
#define WINDOWS_LEAN_AND_MEAN
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>

#include <stdio.h>
#include <string>
#include <vector>
#include <list>
#include <assert.h>

using std::string;
using std::vector;
using std::list;

const int BUG_ME_TEXT_LENGTH = 7;
const char BugMeText[BUG_ME_TEXT_LENGTH] = {'B', 'u', 'g', ' ', 'M', 'e', 0};

typedef list<char> BytesList;

//Eve file byte flags
const unsigned char	IndentChar = 0x20;
const int startText1	= 0x85;
const int startText2	= 0xA0;
const int endText1		= 0x15;
const int endText2		= 0;

const string df2EveFilesPath(			"DF2Files\\Eve\\");
const string df2EveJapDumpPath(			"DF2Files\\EveJapaneseText\\");
const string df2EnglishPath(			"DF2Files\\EveEnglishText\\");
const string df2EveStringInfoPath(		"DF2Files\\EveStringInfo\\");
const string df2TranslatedPath(			"DF2Files\\EveTranslated\\");
const string df2PointerLogPath(			"DF2Files\\EveLog\\PointerFixups\\");
const string df2TextInsertionLogPath(	"DF2Files\\EveLog\\TextInsertions\\");

#define FPUTC_VERIFIED(c, file) if( fputc(c, file) == EOF ) {__debugbreak();}
#define FGETC_VERIFIED(c, file) c = fgetc(file); if(c == EOF) {__debugbreak();}

//#define STOP_AT_LINE 7

struct StringInsertionLocation
{
	int						address;
	BytesList::iterator		insertionPoint;
};

struct OriginalStringInfo
{
	int numBytes;
	int address;

	OriginalStringInfo() : numBytes(0)
	{}
};
typedef vector<OriginalStringInfo> OSIVector;

static bool bFirstPointerFound = 0;

struct PointerInfo
{
	BytesList::iterator pointer;
	BytesList::iterator pointerStart;
	int offset;
};
typedef vector<PointerInfo> PointerVector;

struct OrigAddressInfo
{
	OrigAddressInfo(unsigned char inFB, unsigned char inSB, unsigned char inNFB, unsigned char inNSB, unsigned short inNewLoc, vector<int> &inOrigBytes, 
					const BytesList::iterator inPointerStart, const BytesList::iterator& inPointer) :	firstByte(inFB), secondByte(inSB), 
																										newFirstByte(inNFB), newSecondByte(inNSB),
																										newLoc(inNewLoc),
																										origBytes(inOrigBytes),
																										pointerStart(inPointerStart),
																										pointer(inPointer){}

	unsigned int		firstByte;
	unsigned int		secondByte;
	unsigned int		newFirstByte;
	unsigned int		newSecondByte;
	unsigned short		newLoc;
	BytesList::iterator pointerStart;
	BytesList::iterator pointer;
	vector<int>			origBytes;
};

//Util function to grab all files from a directory
void GetFilesInDir(const string &inDirPath, const char *pExtension, vector<string> &outFiles)
{
	WIN32_FIND_DATA fileData;

	string fileLocation = inDirPath + string("\\*");

	//get the first file in the directory
	HANDLE result = FindFirstFile(fileLocation.c_str(), &fileData);
	
	const size_t extensionLen = strlen(pExtension);

	while(result != INVALID_HANDLE_VALUE)
	{
		unsigned nameLength = static_cast<unsigned> (strlen(fileData.cFileName));

		//skip if the file is just a '.'
		if(fileData.cFileName[0] == '.')
		{
			if(!FindNextFile(result, &fileData))
				break;
			continue;
		}
	
		const size_t fileNameLen = strlen(fileData.cFileName);
		if( fileNameLen <= extensionLen + 2 )
		{
			FindNextFile(result, &fileData);
			continue;
		}

		bool bValidFile = true;
		for(size_t extensionIndex = 1; extensionIndex <= extensionLen; ++extensionIndex)
		{
			if(fileData.cFileName[fileNameLen-extensionIndex] != pExtension[extensionLen-extensionIndex])
			{
				bValidFile = false;
				break;
			}
		}
		if(!bValidFile)
		{
			if(!FindNextFile(result, &fileData))
				break;
			continue;
		}

		string filePath = std::string(fileData.cFileName);
		size_t extPos = filePath.rfind(".");
		outFiles.push_back( filePath.substr(0, extPos) );
		
		if(!FindNextFile(result, &fileData))
			break;
	}
}

//Dump out Japanese text from eve files
void DumpJapaneseText()
{
	//Kana lookup table
	unsigned char translationTable[512];
	FILE *pIndexFile = NULL;
	fopen_s(&pIndexFile, "DF2Files\\Misc\\SPE_MAIN.bin", "rb");
	
	//Move to the start of the dictionary
	int result = fseek(pIndexFile, 0xBE0, SEEK_SET);
	assert(result == 0);

	//Read in the table
	const size_t numItemsToReadFromTranslationTable = 1;
	result = (int)fread(translationTable, (size_t)sizeof(translationTable), numItemsToReadFromTranslationTable, pIndexFile);
	assert(result == numItemsToReadFromTranslationTable);

	//Grab all .eve flies
	vector<string> eveFiles;
	GetFilesInDir(df2EveFilesPath.c_str(), "EVE", eveFiles);

	//Go through all eve files
	const string eveExtension(".EVE");
	const string japDumpExtension(".txt");
	for(size_t currFile = 0; currFile < eveFiles.size(); ++currFile)
	{
		const string eveFileName		= df2EveFilesPath	+ eveFiles[currFile] + eveExtension;
		const string japDumpFileName	= df2EveJapDumpPath + eveFiles[currFile] + japDumpExtension;
		FILE *pInFile					= NULL;
		FILE *pOutFile					= NULL;
		
		fopen_s(&pInFile,  eveFileName.c_str(), "rb");
		fopen_s(&pOutFile, japDumpFileName.c_str(), "w");
		
		assert(pInFile);
		assert(pOutFile);

		if(pInFile)
		{
			int currByte = 0;
			bool bStringStarted1 = false;
			bool bStringStarted2 = false;
			bool bStringEndStarted = false;

			int lineCount = 0;
			int byteCount = -1;
			while(currByte != EOF)
			{
				int prevByte = currByte;
				currByte = fgetc(pInFile);
				++byteCount;

				if(currByte == EOF)
					break;

				//See if we are at the start of a string
				if(!bStringStarted2 && currByte == startText1)
					bStringStarted1 = true;

				//See if this is the second start flag
				else if(bStringStarted1 && !bStringStarted2 && currByte == startText2)
				{
					//The string will start at the next byte
					bStringStarted2 = true;
					continue;
				}

				//If we thought a string was going to start but it didn't at this byte, the prev byte wasn't a start flag, so reset flags
				else if(bStringStarted1 && !bStringStarted2) 
				{
					bStringStarted1 = bStringStarted2 = false;
				}

				//See if we are at the end of a string
				if(bStringStarted1 && bStringStarted2)
				{
					if(currByte == endText1)
					{
						bStringEndStarted = true;
					}
					else if(currByte == endText2 && bStringEndStarted)
					{
						bStringStarted1 = bStringStarted2 = bStringEndStarted = false;

	#ifdef STOP_AT_LINE
						fprintf(pOutFile, " line(%i)\n", lineCount);
	#else
						//Create a comma seperated file so it can easily go into excel
				//		FPUTC_VERIFIED(',',  pOutFile);
						FPUTC_VERIFIED('\n', pOutFile);
	#endif
						++lineCount;
					}
					else //This byte is part of a string
					{
						bStringEndStarted = false;
						assert(currByte >= 0 && currByte < 255 );

	#ifdef STOP_AT_LINE
						if(lineCount == STOP_AT_LINE)
						{
							int k = 0;
							++k;
						}
	#endif

						//If values are between a certain range, then this byte is an index into the kana translation table
						if( (currByte >= 0x40 && currByte <= 0x7F) || (currByte >= 0xA0 && currByte <= 0xDF) )
						{
	#ifdef STOP_AT_LINE
							if(lineCount == STOP_AT_LINE)
	#endif
							{
								//japanese dump
								FPUTC_VERIFIED(translationTable[currByte*2], pOutFile);
								FPUTC_VERIFIED(translationTable[currByte*2 + 1], pOutFile);
							}
						}

						//otherwise this is either a special character or a two byte kanji character
						else
						{
	#ifdef STOP_AT_LINE
							if(lineCount == STOP_AT_LINE)
	#else
							if(1)
	#endif
							{
								//special character like a new line
								if(currByte < 0x40)
								{
									//japanese dump
									//Note: No point in showing carriage returns or other special characters in the translated text, so just skip
									//FPUTC_VERIFIED(currByte, pOutFile);
								}

								//two byte kanji character
								else
								{
									//japanese dump
									FPUTC_VERIFIED(currByte, pOutFile); //write first byte
									FGETC_VERIFIED(currByte, pInFile);  //grab second byte
									FPUTC_VERIFIED(currByte, pOutFile); //write second byte
									++byteCount;
								}
							}

							//only happens if STOP_AT_LINE is set
							else
							{
								if(currByte >= 0x40)
								{
									currByte = fgetc(pInFile);
									++byteCount;
								}
							}
						}//else this is a special character or 2byte kanjii
					}//else this byte is part of a string
				}//if(bStringStarted1 && bStringStarted2)
				
			}//while(currByte != eof)

			fclose(pInFile);
			fclose(pOutFile);

		}//if(pInFile)
	}//for
}

//Finds next pointer in a stream of bytes from an .eve file
bool GetNextPointer(BytesList::iterator &inStream, BytesList::const_iterator &endStream, PointerVector &outPointers)//BytesList::iterator &outPointer, int &inOutCurrByte)
{
#define INCR_STREAM() ++inStream; ++byteOffset;
#define INCR_PEEK() ++peek; ++peekBytes;

	//Pointers come in one of the following formats
	//Type1 = 29 10 nn 00 00 PP PP xx xx 06 PP PP
	//Type2 = 29 10 00 00 ?? 00 ?? 00 07 PP PP
	//Type3 = 2F 10 00 00 00 xx 00 xx 06 PP PP 06 PP PP
	//Type4 = 2F 10 00 00 00 xx 00 06 PP PP 07 PP PP 
	//Type5 = LL 10 xx 00 25 00 PP PP			//LL (29, 2A, 2B) nn between 01 and 09

	int byteOffset = 0;
	PointerInfo newPointer;
	BytesList::iterator pointerStart;

	while(inStream != endStream)
	{
		bool twoF = false;
		bool twoA = false;
		bool twoB = false;
		
		newPointer.offset	= 0;
		pointerStart		= inStream;

		//Not the beginning of a pointer, so just continue
		char c = *inStream;
		if( !(c == (char)0x29 || c == (char)0x2F || c == (char)0x2A || c == (char)0x2B) )
		{
			INCR_STREAM();
			continue;
		}

		switch(c)
		{
			case 0x2F:
				twoF = true;
				break;

			case 0x2A:
				twoA = true;
				break;

			case 0x2B:
				twoB = true;
				break;
		}

		//00->01
		INCR_STREAM();

		//Second byte is not that of a pointer, so just continue
		if( *inStream != (char)0x10 )
		{
			INCR_STREAM();
			continue;
		}
		//01->02
		INCR_STREAM();
			
		//			00 01 02 03 04 05 06 07 08 09 10 11 12
		//Type3 =	2F 10 00 00 00 xx 00 xx 06 PP PP 06 PP PP                                                             
		//Type4 =	2F 10 00 00 00 xx 00 06 PP PP 07 PP PP  				
		if(twoF && *inStream >= 0x00 && *inStream <= 0x09)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;
			
			//02->03
			INCR_PEEK();
			if(*peek != 0)
				goto twoFFail;

			//03->04
			INCR_PEEK();
			if(*peek != 0)
				goto twoFFail;
			
			//04->05
			INCR_PEEK();
			if(*peek != (char)0x87)
				goto twoFFail;

			//05->06
			INCR_PEEK();
			if(*peek != 0)
				goto twoFFail;

			//06->07
			INCR_PEEK();

			//Type4
			if(*peek == 0x06)
			{
				//07->08
				INCR_PEEK();

				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes + byteOffset;
				outPointers.push_back(newPointer);

				//reset counter for next pointer
				peekBytes = 0;

				//08->09
				INCR_PEEK();

				//9->10
				INCR_PEEK();
				if( *peek == (char)0x07 )
				{
					INCR_PEEK();

					newPointer.pointerStart	= pointerStart;
					newPointer.pointer		= peek;
					newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
					outPointers.push_back(newPointer);
				}

				return true;
			}

			//Type3

			//07->08
			INCR_PEEK();
			if(*peek != (char)0x06)
				goto twoFFail;

			//08->09
			INCR_PEEK();
			inStream				= peek;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes + byteOffset;
			newPointer.pointerStart = pointerStart;
			outPointers.push_back(newPointer);

			//reset peek counter for next pointer
			peekBytes = 0;

			//09->10
			INCR_PEEK();

			//10->11
			INCR_PEEK();
			if(*peek == (char)(0x06) )
			{
				INCR_PEEK();
			
				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
				outPointers.push_back(newPointer);
			}

			return true;

twoFFail:
			{}
		}

#if 0
				//			00 01 02 03 04 05 06 07
		//Type?	=	2F 10 xx 00 00 11 10 PP PP           <--I think FC 00 is a pointer
		//			2F 10 01 00 00 11 10 FC 00
		//			2F 10 01 00 00 11 10 FC 00 00 
		//			2F 10 15 00 00 11 10 C7 00 
		if(twoF && *inStream >= 0x00 && *inStream <= 0x15)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//2->3
			INCR_PEEK();
			if( *peek != 0 )
				goto failTypeQ;

			//3->4
			INCR_PEEK();
			if( *peek != 0 )
				goto failTypeQ;

			//4->5
			INCR_PEEK();
			if( *peek != (char)0x11 )
				goto failTypeQ;

			//5->6
			INCR_PEEK();
			if( *peek != (char)0x10 )
				goto failTypeQ;

			//6->7
			INCR_PEEK();
			
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes + byteOffset;
			outPointers.push_back(newPointer);
			return true;
failTypeQ:
			{}
		}
#endif

		//If this is a Type1 pointer
		//			00 01 02 03 04 05 06 07 08 09 10 11 12
		//Type1 =	29 10 xx 00 00 PP PP xx xx 06 PP PP
		//Type5 =	LL 10 xx 00 25 00 PP PP			//LL (29, 2A, 2B) nn between 01 and 09
		//			29 10 00 00 25 00 6F 
		//02
		if( bFirstPointerFound && !twoF && (*inStream >= 0x00 && *inStream <= 0x09) )
		{
			//02->03
			INCR_STREAM();
			if(*inStream != 0)
				continue;

			//03->04
			INCR_STREAM();			
			if( *inStream != 0)
			{
				//Perhaps this is a Type5
				BytesList::iterator peek = inStream;
				int peekBytes = 0;
				if( *peek == (char)0x25 )
				{
					//04->05
					INCR_PEEK();					
					if( *peek != 0 )
						goto type5Fail;

					//05->06
					INCR_PEEK();
					newPointer.pointerStart = pointerStart;
					newPointer.pointer		= peek;
					newPointer.offset		= byteOffset + peekBytes;
					outPointers.push_back(newPointer);
					return true;
					
type5Fail:
					{}
				}

				INCR_STREAM();
				continue;
			}

			//04->05
			INCR_STREAM();

			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= inStream;
			newPointer.offset		= byteOffset;
			outPointers.push_back(newPointer);

			BytesList::iterator peek = inStream;
			int peekBytes = 0;
			INCR_PEEK(); //5->6
			INCR_PEEK(); //6->7
			INCR_PEEK(); //7->8
			INCR_PEEK(); //8->9
			if(*peek == 0x06)
			{
				INCR_PEEK(); //10->11

				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
				outPointers.push_back(newPointer);
			}

			return true;
		}
		INCR_STREAM();

		if( *inStream != 0x00 )
		{
			continue;
		}

		//Look 7 bytes ahead to see if this is a Type2 pointer
		BytesList::iterator peekAheadIterator = inStream;
		c = *( ++(++(++(++(++peekAheadIterator)))) );
		if( !bFirstPointerFound && c == (char)0x07 )
		{
			bFirstPointerFound = true;
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= ++peekAheadIterator;
			newPointer.offset		= 6 + byteOffset;
			outPointers.push_back(newPointer);
			return true;
		}
/*
		INCR_STREAM();
		if( *inStream != 0 )
			continue;

		//We got ourselves a Type1 iterator
		outPointer = ++inStream;
		++inOutCurrByte

		return true;
*/
		continue;
	}

	return false;
}

int GetPointerOffset(const OSIVector &newStringsInfo, const OSIVector &origStringsInfo, int currByte)
{
	int offset = 0;
	for(size_t i = 0; i < newStringsInfo.size(); ++i)
	{
		const OriginalStringInfo &newStringInfo = newStringsInfo[i];

		if(newStringInfo.address < currByte)
		{
			const OriginalStringInfo &origStringInfo = origStringsInfo[i];

			offset += newStringInfo.numBytes - origStringInfo.numBytes;
		}
	}

	return offset;
}

//Inserts English text into an eve file
void InsertEnglishText()
{
	//Kana lookup table
	unsigned char translationTable[512];
	FILE *pIndexFile = NULL;
	fopen_s(&pIndexFile, "DF2Files\\Misc\\SPE_MAIN.bin", "rb");
	
	//Move to the start of the dictionary
	int result = fseek(pIndexFile, 0xBE0, SEEK_SET);
	assert(result == 0);

	//Read in the table
	const size_t numItemsToReadFromTranslationTable = 1;
	result = (int)fread(translationTable, sizeof(translationTable), numItemsToReadFromTranslationTable, pIndexFile);
	assert(result == numItemsToReadFromTranslationTable);

	//Grab all .eve flies
	vector<string> eveFiles;
	GetFilesInDir(df2EveFilesPath.c_str(), "EVE", eveFiles);

	//Go through all eve files
	const string eveExtension(".EVE");
	const string engExtension(".txt");
	const string logExtension(".txt");

	for(size_t currFile = 0; currFile < eveFiles.size(); ++currFile)
	{
		//Open file with translated text, if it doesn't exist, then skip translating the eve file
		const string engFileName		= df2EnglishPath	+ eveFiles[currFile] + engExtension;
		FILE *pInEngFile				= NULL;
		fopen_s(&pInEngFile, engFileName.c_str(), "rb");
		if(!pInEngFile)
			continue;

		const string eveFileName			= df2EveFilesPath	+ eveFiles[currFile] + eveExtension;
		const string translatedFileName		= df2TranslatedPath + eveFiles[currFile] + eveExtension;
		const string pointerLogFileName		= df2PointerLogPath + eveFiles[currFile] + logExtension;
		const string textInsertLogFileName	= df2TextInsertionLogPath + eveFiles[currFile] + logExtension;
		FILE *pInEveFile					= NULL;
		FILE *pOutEveFile					= NULL;
		FILE *pPointerLogFile				= NULL;
		FILE *pTextInsertLogFile			= NULL;
		
		fopen_s(&pInEveFile,			eveFileName.c_str(), "rb");
		fopen_s(&pOutEveFile,			translatedFileName.c_str(), "wb");
		fopen_s(&pPointerLogFile,		pointerLogFileName.c_str(), "w");
		fopen_s(&pTextInsertLogFile,	textInsertLogFileName.c_str(), "w");
		
		assert(pInEveFile);
		assert(pOutEveFile);

		bool bStringStarted1	= false;
		bool bStringStarted2	= false;
		bool bStringEndStarted	= false;
		int	 currByte			= 0;
		int  byteCount			= 0;
		char engStrBuffer[1024];
		char *inEnglishStrBuffer = engStrBuffer;

		BytesList fileBytes;
		vector< StringInsertionLocation > stringInsertionPoints;
		OSIVector			origStringsInfo;
		OriginalStringInfo	currStringInfo;

		//if this is a START_ eve file, then there is a special kind of pointer at the start
		if( eveFileName.find("START_") == string::npos )
			bFirstPointerFound = true;
		else
			bFirstPointerFound = false;

		while(currByte != EOF)
		{
			currByte = fgetc(pInEveFile);
			if(currByte == EOF)
				break;

			++byteCount;

			//See if we are at the start of a string
			if(!bStringStarted2 && currByte == startText1)
			{
				bStringStarted1 = true;
			}
			//See if this is the second start flag
			else if(bStringStarted1 && !bStringStarted2 && currByte == startText2)
			{
				currStringInfo.numBytes = 0;

				//The string will start at the next byte
				bStringStarted2 = true;
				fileBytes.push_back(IndentChar);

				assert(stringInsertionPoints.size() == origStringsInfo.size());

				//Keep track of where this string used to be so we can insert our translated text into it
				StringInsertionLocation locInfo;
				locInfo.address			= byteCount;
				locInfo.insertionPoint	= --fileBytes.end();
				stringInsertionPoints.push_back( locInfo );
				continue;
			}
			//If we thought a string was going to start but it didn't at this byte, the prev byte wasn't a start flag, so reset flags
			else if(bStringStarted1 && !bStringStarted2) 
			{
				currStringInfo.numBytes = 0;
				bStringStarted1 = bStringStarted2 = false;
			}

			//See if we are at the end of a string
			bool bStringEnded = false;
			if(bStringStarted1 && bStringStarted2)
			{
				if(currByte == endText1)
				{
					bStringEndStarted = true;
				}
				else if(currByte == endText2 && bStringEndStarted)
				{
					bStringStarted1 = bStringStarted2 = bStringEndStarted = false;
					bStringEnded = true;

					//add info about the string that was found
					origStringsInfo.push_back(currStringInfo);
				}
				else //This byte is part of a string
				{
					++currStringInfo.numBytes;
				}

				if(bStringEnded)
				{
					fileBytes.push_back( (char)endText1 );
					fileBytes.push_back( 0 );
				}
			}
			else
			{
				fileBytes.push_back( (char)currByte );
			}
		}//end while(currByte != EOF)

		//Now fill in the strings
		OSIVector			newStringsInfo;
		OriginalStringInfo	newStringInfo;
		int count = 0;
		for(size_t insertPoint = 0; insertPoint < stringInsertionPoints.size(); ++insertPoint)
		{			
			//See if we have  a translated string, if we don't just fill in "Bug Me"
			char *retValue = fgets(inEnglishStrBuffer, 1024, pInEngFile);
			if(retValue == 0)
				memcpy(inEnglishStrBuffer, BugMeText, BUG_ME_TEXT_LENGTH*sizeof(char));

			inEnglishStrBuffer = strtok(inEnglishStrBuffer, "\n\r\v");
			size_t strLen = strlen(inEnglishStrBuffer);

			++count;

			//insertion itertator starts at the A0 in 85A0, so we want one past that
			list<char>::iterator insertionPoint = stringInsertionPoints[insertPoint].insertionPoint;
			insertionPoint++;

			newStringInfo.numBytes = 0;
			newStringInfo.address = stringInsertionPoints[insertPoint].address;

			//Insert the translated string
			int stringsPrinted = 1; //already have an indent
			int totalPrinted = 1;
			int numLines = 0;
			int wordStartIndex = -1;
			list<char>::iterator wordStartInsertionPoint;

			char tmp[1024];
			int tmpCount = 0;
			memset(tmp, 0, sizeof(tmp));
			for(size_t i = 0; i < strLen; ++i)
			{
				if( totalPrinted == 36*3 )
					break;

				assert(totalPrinted < 36*3);

				const char currLetter = inEnglishStrBuffer[i];

				//discard new lines and what not
				if(currLetter < 0x20)
					continue;

				//insert our new string
				insertionPoint = fileBytes.insert( insertionPoint, currLetter);
				tmp[tmpCount++] = currLetter;

				//save off start of the word
				if(currLetter != ' ')
				{
					if(wordStartIndex == -1)
					{
						wordStartIndex = (int)i;
						wordStartInsertionPoint = insertionPoint;
					}
				}
				else
					wordStartIndex = -1;

				insertionPoint++;
				newStringInfo.numBytes++;
				totalPrinted++;

				if( totalPrinted == 36*3 )
					break;

				if(++stringsPrinted >= 36)
				{
					if(++numLines == 3)
						break;

					stringsPrinted = 1;

					//insert new line
					if( wordStartIndex > -1 )
					{
						fileBytes.insert(wordStartInsertionPoint, 0x0D);
						tmp[tmpCount++] = 0x0D;
						stringsPrinted += (int)i - wordStartIndex;
						assert(i - wordStartIndex >= 0);
					}
					else					
					{
						insertionPoint = fileBytes.insert( insertionPoint, 0x0D);
						insertionPoint++;
						tmp[tmpCount++] = 0x0D;
					}

					newStringInfo.numBytes++;
					totalPrinted++;
				}
			}

			if(totalPrinted < (int)strLen)
			{
				fprintf(pTextInsertLogFile, "Did not fit(Length:%i): %s\n", strLen, inEnglishStrBuffer);
			}

			//Store info about the new string
			newStringsInfo.push_back(newStringInfo);
		}

		//We should have same amount of new strings as original ones
		assert(origStringsInfo.size() == newStringsInfo.size());

		//find all the pointers
		currByte = 0;

		//store original info for LogFile
		vector<OrigAddressInfo> logInfo;

		for( BytesList::iterator bytesIter = fileBytes.begin(); bytesIter != fileBytes.end(); ++bytesIter, ++currByte)
		{
			PointerVector outPointers;

			if( GetNextPointer(bytesIter, fileBytes.end(), outPointers) == false)
				break;

			//Bytes will change in the loop below, so store off original data first
			vector<int> origBytes;
			if(outPointers.size() > 0)
			{
				size_t lastPointerIndex = outPointers.size() - 1;
				const PointerInfo &ptr	= outPointers[lastPointerIndex];

				BytesList::iterator ptrStart = ptr.pointerStart;
				while(ptrStart != ptr.pointer)
				{
					origBytes.push_back( (unsigned char)(*ptrStart) );					
					++ptrStart;
				}
			}

			for(size_t i = 0; i < outPointers.size(); ++i)
			{
				bytesIter		 = outPointers[i].pointer;
				currByte		 += outPointers[i].offset;

				//Little endian byte order for pointers
				char &secondByte = *bytesIter; ++bytesIter; ++currByte;
				char &firstByte  = *bytesIter;
				int address      = (firstByte << 8) | (secondByte & 0xff);

				int offset = GetPointerOffset(newStringsInfo, origStringsInfo, address);
				address += offset;

				char orgFirstByte	= firstByte;
				char orgSecondByte	= secondByte; 
				firstByte			= address >> 8;
				secondByte			= address & 0xff;

				logInfo.push_back( OrigAddressInfo(orgFirstByte, orgSecondByte, firstByte, secondByte, currByte-1, origBytes, outPointers[i].pointerStart, outPointers[i].pointer) );
			}			
		}

		//Save log file
		fprintf(pPointerLogFile, "Little Endian           Big Endian		NewValue						Full\n");
		for(size_t i = 0; i < logInfo.size(); ++i)
		{
			OrigAddressInfo &currInfo = logInfo[i];
			
			fprintf(pPointerLogFile, "%.2X %.2X                   %.2X %.2X			%.2X %.2X (@%.4X)	 			",
				currInfo.secondByte, currInfo.firstByte, currInfo.firstByte, currInfo.secondByte, currInfo.newFirstByte, currInfo.newSecondByte, currInfo.newLoc);

			int byteIndex = 0;
			while(currInfo.pointerStart != currInfo.pointer)
			{
				fprintf(pPointerLogFile, "%.2X ", currInfo.origBytes[byteIndex++]);
				++currInfo.pointerStart;
			}
			fprintf(pPointerLogFile, "%.2X %.2X\n", currInfo.secondByte, currInfo.firstByte);
		}
		
		//write out translated file
		int counter = 0;
		for( BytesList::const_iterator newBytesIter = fileBytes.begin(); newBytesIter != fileBytes.end(); ++newBytesIter)
		{
			fputc( (char)*newBytesIter, pOutEveFile);
			++counter;
		}

		fclose(pInEngFile);
		fclose(pInEveFile);
		fclose(pOutEveFile);
		fclose(pPointerLogFile);
		fclose(pTextInsertLogFile);
	}//for(eve files)
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#if _DEBUG
	_CrtSetDbgFlag( _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF );
	//	_CrtSetBreakAlloc(404);
#endif

//	DumpJapaneseText();
	InsertEnglishText();

	return 0;
}
