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

const string df2EveFilesPath(		"DF2Files\\Eve\\");
const string df2EveJapDumpPath(		"DF2Files\\EveJapaneseText\\");
const string df2EnglishPath(		"DF2Files\\EveEnglishText\\");
const string df2EveStringInfoPath(	"DF2Files\\EveStringInfo\\");
const string df2TranslatedPath(		"DF2Files\\EveTranslated\\");

#define FPUTC_VERIFIED(c, file) if( fputc(c, file) == EOF ) {__debugbreak();}
#define FGETC_VERIFIED(c, file) c = fgetc(file); if(c == EOF) {__debugbreak();}

//#define STOP_AT_LINE 7

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
			continue;

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
	BytesList::iterator iter;
	int offset;
};
typedef vector<PointerInfo> PointerVector;

//Finds next pointer in a stream of bytes from an .eve file
bool GetNextPointer(BytesList::iterator &inStream, BytesList::const_iterator &endStream, PointerVector &outPointers)//BytesList::iterator &outPointer, int &inOutCurrByte)
{
#define INCR_STREAM() ++inStream; ++byteOffset;
#define INCR_PEEK() ++peek; ++peekBytes;

	//Pointers come in one of the following formats
	//Type1 = 29 10 xx 00 00 PP PP xx xx 06 PP PP
	//Type2 = 29 10 00 00 ?? 00 ?? 00 07 PP PP
	//Type3 = 2F 10 00 00 00 xx 00 xx 06 97 PP                                                             
	//Type4 = 2F 10 00 00 00 xx 00 06 PP PP   
	
	int byteOffset = 0;
	PointerInfo newPointer;

	while(inStream != endStream)
	{
		bool twoF = false;
		newPointer.offset = 0;

		//Not the beginning of a pointer, so just continue
		if( *inStream != (char)0x29 && *inStream != (char)0x2F )
		{
			INCR_STREAM();
			continue;
		}
		if( *inStream == (char)0x2F )
			twoF = true;

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
			
		//	00 01 02 03 04 05 06 07 08 09 10
		//	2F 10 00 00 00 87 00 9B 06 97 00                                                             
		//	2F 10 00 00 00 87 00 06 97 00   
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
			if(*peek == 0x06)
			{
				//07->08
				INCR_PEEK();

				inStream = peek;
				newPointer.iter = inStream;
				newPointer.offset = peekBytes + byteOffset;
				outPointers.push_back(newPointer);
				return true;
			}

			//07->08
			INCR_PEEK();
			if(*peek != (char)0x06)
				goto twoFFail;


			//08->09
			INCR_PEEK();
			inStream = peek;
			newPointer.iter = peek;
			newPointer.offset = peekBytes + byteOffset;
			outPointers.push_back(newPointer);
			return true;

twoFFail:
			{}
		}

		//If this is a Type1 pointer
		//			00 01 02 03 04 05 06 07 08 09 10 11 12
		//Type1 =	29 10 xx 00 00 PP PP xx xx 06 PP PP
		//02
		if( bFirstPointerFound && *inStream >= 0x00 && *inStream <= 0x09)
		{
			//02->03
			INCR_STREAM();
			if(*inStream != 0)
				continue;

			//03->04
			INCR_STREAM();
			
			if( *inStream != 0)
			{
				INCR_STREAM();
			//	if(*inStream != 0)
				continue;
			}

			//04->05
			INCR_STREAM();

			//found the pointer
			newPointer.iter		= inStream;
			newPointer.offset	= byteOffset;
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

				newPointer.iter = peek;
				newPointer.offset += peekBytes;
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
		char c = *( ++(++(++(++(++peekAheadIterator)))) );
		if( !bFirstPointerFound && c == (char)0x07 )
		{
			bFirstPointerFound = true;
			newPointer.iter = ++peekAheadIterator;
			newPointer.offset = 6 + byteOffset;
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

	for(size_t currFile = 0; currFile < eveFiles.size(); ++currFile)
	{
		//Open file with translated text, if it doesn't exist, then skip translating the eve file
		const string engFileName		= df2EnglishPath	+ eveFiles[currFile] + engExtension;
		FILE *pInEngFile				= NULL;
		fopen_s(&pInEngFile, engFileName.c_str(), "rb");
		if(!pInEngFile)
			continue;

		const string eveFileName		= df2EveFilesPath	+ eveFiles[currFile] + eveExtension;
		const string translatedFileName = df2TranslatedPath + eveFiles[currFile] + eveExtension;
		FILE *pInEveFile				= NULL;
		FILE *pOutEveFile				= NULL;
		
		fopen_s(&pInEveFile,  eveFileName.c_str(), "rb");
		fopen_s(&pOutEveFile, translatedFileName.c_str(), "wb");
		
		assert(pInEveFile);
		assert(pOutEveFile);

		bool bStringStarted1	= false;
		bool bStringStarted2	= false;
		bool bStringEndStarted	= false;
		int	 currByte			= 0;
		int  byteCount			= 0;
		char inEnglishStrBuffer[1024];

		BytesList fileBytes;
		vector< StringInsertionLocation > stringInsertionPoints;
		OSIVector			origStringsInfo;
		OriginalStringInfo	currStringInfo;

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
		const char *testStr = "abcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyzabcdefghijklmnopqrstuvqxyz";
		for(size_t insertPoint = 0; insertPoint < stringInsertionPoints.size(); ++insertPoint)
		{			
			//See if we have  a translated string, if we don't just fill in "Bug Me"
			char *retValue = fgets(inEnglishStrBuffer, 1024, pInEngFile);
			if(retValue == 0)
				memcpy(inEnglishStrBuffer, BugMeText, BUG_ME_TEXT_LENGTH*sizeof(char));

			size_t strLen = strlen(inEnglishStrBuffer);
			if(strLen < (size_t)origStringsInfo[count].numBytes)
			{
	//			int k =0;
	//			++k;
	//			strLen = origStringsInfo[count].numBytes;
	//			memcpy(inEnglishStrBuffer, testStr, strLen);
			}

			//String can't go beyond 36 characters including the initial indent (0x20)
			if(strLen >= 36*3)
				strLen = 36*3 - 1;
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
			unsigned char tmp[1024];
			memset(tmp, 0, sizeof(tmp));
			for(size_t i = 0; i < strLen-2; ++i)
			{
				if(totalPrinted >= (36*3-4))
					break;

				//discard new lines and what not
				if(inEnglishStrBuffer[i] < 0x20)
					continue;

				tmp[totalPrinted-1] = inEnglishStrBuffer[i];

				newStringInfo.numBytes++;
				totalPrinted++;

				//insert our new string
				insertionPoint = fileBytes.insert( insertionPoint, inEnglishStrBuffer[i]);
				insertionPoint++;

				if(totalPrinted >= (36*3-4))
					break;

				if(++stringsPrinted == 32)
				{
					stringsPrinted = 0;
					newStringInfo.numBytes++;
					totalPrinted++;

					tmp[totalPrinted-1] = 0x0D;

					if(++numLines == 3)
						break;

					//insert new line
					insertionPoint = fileBytes.insert( insertionPoint, 0x0D);
					insertionPoint++;
				}
			}

			//Store info about the new string
			newStringsInfo.push_back(newStringInfo);
		}

		//We should have same amount of new strings as original ones
		assert(origStringsInfo.size() == newStringsInfo.size());

		//find all the pointers
		currByte = 0;

		for( BytesList::iterator bytesIter = fileBytes.begin(); bytesIter != fileBytes.end(); ++bytesIter, ++currByte)
		{
			PointerVector outPointers;

			if( GetNextPointer(bytesIter, fileBytes.end(), outPointers) == false)
				break;

			for(size_t i = 0; i < outPointers.size(); ++i)
			{
				bytesIter		 = outPointers[i].iter;
				currByte		 += outPointers[i].offset;

				//Little endian byte order for pointers
				char &secondByte = *bytesIter; ++bytesIter; ++currByte;
				char &firstByte  = *bytesIter;
				int address      = (firstByte << 8) | (secondByte & 0xff);

				int offset = GetPointerOffset(newStringsInfo, origStringsInfo, address);
				address += offset;
	 
				firstByte	= address >> 8;
				secondByte	= address & 0xff;
			}			
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
