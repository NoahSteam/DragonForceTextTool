#include <windows.h>
#include <windowsx.h>

#include <stdio.h>
#include <string>
#include <vector>
#include <list>
#include <assert.h>
#include <time.h>
#include <direct.h>

using std::string;
using std::vector;
using std::list;

#define FSEEK_SUCCESS 0

const int BUG_ME_TEXT_LENGTH = 7;
const char BugMeText[BUG_ME_TEXT_LENGTH] = {'B', 'u', 'g', ' ', 'M', 'e', 0};

typedef list<char> BytesList;

//Eve file byte flags
const unsigned char	IndentChar = 0x20;
const int startText1	= 0x85;
const int startText1_b	= 0xBE;
const int startText2	= 0xA0;
const int startText2_b	= 0x88;
const int startText2_c  = 0x81;
const int startText2_d  = 0x20;
const int startText2_e  = 0xDE;
const int startText2_f  = 0x5A;
const int startText2_g  = 0xA2;
const int endText1		= 0x15;
const int endText2		= 0;

const string df2BinFilesPath(			"DF2Files\\Bin\\");
const string df3BinEngPath(				"DF2Files\\BinEnglishText\\");
const string df2BinJapDumpPath(			"DF2Files\\BinJapaneseText\\");
const string df2BinPatchPath(			"DF2Files\\BinPatched\\");

const string df2EveFilesPath(			"DF2Files\\Eve\\");
const string df2EveJapDumpPath(			"DF2Files\\EveJapaneseText\\");
const string df2EnglishPath(			"DF2Files\\EveEnglishText\\");
const string df2EveStringInfoPath(		"DF2Files\\EveStringInfo\\");
const string df2TranslatedPath(			"DF2Files\\EveTranslated\\");
const string df2LogPath(				"DF2Files\\EveLog\\");
const string df2PointerLogPath(			"PointerFixups\\");
const string df2TextInsertionLogPath(	"TextInsertions\\");
const string df2PossiblePointersLogPath("PossiblePointers\\");

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

static bool bFirstPointerFound	= false;
static bool bSpeechFile			= false;
static bool bFieldXXFile		= false;
static bool bDungeonFile		= false;
static bool bStartFile			= false;

struct PointerInfo
{
	BytesList::iterator pointer;
	BytesList::iterator pointerStart;
	int offset;
	string pointerType;
};
typedef vector<PointerInfo> PointerVector;

struct OrigAddressInfo
{
	OrigAddressInfo(unsigned char inFB, unsigned char inSB, unsigned char inNFB, unsigned char inNSB, unsigned short inNewLoc, vector<int> &inOrigBytes, 
					const BytesList::iterator inPointerStart, const BytesList::iterator& inPointer, string &inPointerType) :	
																										firstByte(inFB), secondByte(inSB), 
																										newFirstByte(inNFB), newSecondByte(inNSB),
																										newLoc(inNewLoc),
																										address( (inNFB << 8) | (inSB & 0xff) ),
																										origBytes(inOrigBytes),
																										pointerStart(inPointerStart),
																										pointer(inPointer),
																										pointerType(inPointerType){}
	unsigned int		firstByte;
	unsigned int		secondByte;
	unsigned int		newFirstByte;
	unsigned int		newSecondByte;
	unsigned short		newLoc;
	unsigned int		address;
	BytesList::iterator pointerStart;
	BytesList::iterator pointer;
	vector<int>			origBytes;
	string				pointerType;
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

	int ss = 0;

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

		if(eveFileName.find("FIELD_") == string::npos)
			bFieldXXFile = false;
		else
			bFieldXXFile = true;

		if(pInFile)
		{
			int currByte = 0;
			bool bStringStarted1 = false;
			bool bStringStarted2 = false;
			bool bStringEndStarted = false;

			int lineCount = 0;
			int byteCount = -1;
			int startTextByteIndex = 0;

			while(currByte != EOF)
			{
				int prevByte = currByte;
				currByte = fgetc(pInFile);
				++byteCount;

				if(currByte == EOF)
					break;

				//See if we are at the start of a string
				if(!bStringStarted2 && (currByte == startText1 || currByte == startText1_b) )
				{
					if(startTextByteIndex == -1 && currByte == startText1_b)
						startTextByteIndex = byteCount;
					bStringStarted1 = true;
				}

				//See if this is the second start flag
				else if(bStringStarted1 && !bStringStarted2 && (currByte == startText2 || currByte == startText2_b || currByte == startText2_c || (currByte == startText2_d && bFieldXXFile) || currByte == startText2_e || currByte == startText2_f || currByte == startText2_g) )
				{
					//The string will start at the next byte
					bStringStarted2 = true;

					if( !(currByte == startText2_b || currByte == startText2_c || currByte == startText2_d || currByte == startText2_e || currByte == startText2_f || currByte == startText2_g) )
					{
						continue;
					}
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
					else if(currByte == endText2)// && bStringEndStarted)
					{
						startTextByteIndex = -1;
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
#if FIND_MISSING_STRINGS
				else
				{
					if( startTextByteIndex > -1 && prevByte == endText1 && currByte == endText2 )
					{
						byteCount = startTextByteIndex;
						fseek(pInFile, byteCount, SEEK_SET);

						FGETC_VERIFIED(currByte, pInFile);
						assert(currByte == startText1_b);

						bStringStarted1 = bStringStarted2 = true;
						fprintf(pOutFile, "New: ");
					}
				}
#endif

			}//while(currByte != eof)

			fclose(pInFile);
			fclose(pOutFile);

		}//if(pInFile)
	}//for

	ss = ss+1;
}

//Finds next pointer in a stream of bytes from an .eve file
bool GetNextPointer(BytesList::iterator &inStream, BytesList::const_iterator &endStream, PointerVector &outPointers)//BytesList::iterator &outPointer, int &inOutCurrByte)
{
#define INCR_STREAM() ++inStream; ++byteOffset; if(inStream == endStream) return false; 
#define INCR_PEEK() ++peek; ++peekBytes; if(peek == endStream) return false; 

	//Pointers come in one of the following formats
	//Type1   = 29 10 xx 00 00 PP PP xx xx 06 PP PP
	//Type2   = 29 10 xx 00 00 PP PP 80 07 PP PP
	//Type3   = 2F 10 00 00 00 87 00 xx 06 PP PP kk PP PP //kk is 06 or 07	
	//Type4   = 2F 10 00 00 00 87 00 06 PP PP 07 PP PP 
	//Type4b  = 2F 10 xx 00 00 06 PP PP 06 PP PP
	//Type5   = LL 10 xx 00 25 00 PP PP 06 PP	//LL (29, 2A, 2B) nn between 01 and 09
	//Type6   = BA xx xx 00 07 PP PP				//Only in SPEECH files
	//Type7   = 86 B6 01 01 01 06 PP PP 07 PP PP 06 PP PP 06 PP PP 07 PP PP 06 PP PP 06 PP PP //Only in SPEECH files	
	//Type8   = 2E 10 00 00 00 PP PP
	//Type8b  = 2E 10 01 00 00 4E 00 07 PP PP
	//Type9   = AF xx 07 PP PP nn 07 PP PP		//nn = 86 or 88
	//Type10  = C1 03 00 00 06 PP PP				//Only in FIELD_xx files
	//Type11  = ss 06 PP PP						//ss = 0, in FIELD_XX can be 94, C0, 1E
	//Type12  = 02 06 PP PP						//Only in SPEECH files
	//Type12b = 02 xx 06 PP PP
	//Type12c = 02 xx xx 06 PP PP
	//Type13  = 2F 10 xx 00 00 88 PP
	//Type14  = 15 00 06 PP PP 06 PP PP 
	//Type15  = LL 10 xx 00 00 PP PP 06 PP PP
	//Type16  = 2F 10 02 00 00 xx 80 07 pp pp  (found in Start so far)
	//Type17  = 29 10 xx 00 00 PP PP 07 PP PP
	//Type18  = B5 xx 00 06 PP PP //Dungeon files
	//Type19  = 2F 14 10 04 00 00 00 06 PP PP 06 PP PP	(Dungeon)
	//Type20  = 29 14 10 02 00 00 00 PP PP 				(Dungeon
	//Type21  = B7 03 06 PP PP 06 PP PP 07 PP PP 06 PP PP 06 PP PP 07 PP PP 06 PP PP 06 PP PP
	//Type22  = 2F 10 01 00 00 xx 06 PP PP  (Speech)  
	//Type23  = 2F 10 01 00 00 06 PP PP 06 PP PP (Speech || Start)	
	//Type24  = 15 00 92 86 94 06 PP PP (START_01)
	//Type25  = 15 00 81 86 8E 14 80 06 PP PP (START_02)
	//Type26  = 15 00 92 86 81 8E 14 80 06 PP PP (START_05)
	//Type27  = 29 10 00 00 00 PP PP B6 01 02 01 06 PP PP 06 PP PP (Dungeon and Speech) 
	//Type27b = 29 10 00 00 00 PP PP 9F 03 00 06 PP PP
	//Type28  = 29 10 01 00 00 PP PP 07 PP PP
	//Type29  = 15 00 xx xx 06 PP PP (Speech)
	//Type30  = 15 00 nn 06 PP PP  //nn is 92 or BF
	//Type31  = 2F 10 xx 00 00 xx xx xx 00 xx 06 PP PP 06 PP PP
	//TODO:   = 2B 10 00 00 PP PP		

	int byteOffset = 0;
	PointerInfo newPointer;
	BytesList::iterator pointerStart;

	while(inStream != endStream)
	{
		bool twoF		= false;
		bool twoA		= false;
		bool twoB		= false;
		bool twoE		= false;
		bool two9		= false;
		bool speechBA	= false;
		bool speech86	= false;
		bool speechAF	= false;
		bool fieldC1	= false;
		bool _150006	= false;
		bool two6		= false;
		bool zero6		= false;
		bool ca			= false;
		bool b5			= false;
		bool b7			= false;

		newPointer.offset	= 0;
		pointerStart		= inStream;

		//Not the beginning of a pointer, so just continue
		char c = *inStream;
		if( !(
				c == (char)0x29 || 
				c == (char)0x2F || 
				c == (char)0x2A ||
				c == (char)0x2B ||
				c == (char)0x15 ||
				(c == (char)0xCA && bFieldXXFile) ||
				(c == (char)0xB7 && (bDungeonFile || bSpeechFile)) ||
				(c == (char)0xB5 && bDungeonFile) ||
				(c == (char)0x00 && (bSpeechFile || bFieldXXFile) ) ||
				(c == (char)0x2E && (bSpeechFile || bDungeonFile || bStartFile) ) || 
				(c == (char)0x02 && (bSpeechFile  || bDungeonFile)) ||
				(c == (char)0xAF && (bSpeechFile  || bDungeonFile)) ||
				(c == (char)0xBA && bSpeechFile) ||
				(c == (char)0x86 && (bSpeechFile  || bDungeonFile)) ||
				( (c == (char)0x94 || c == (char)0xC0 || c == (char)0x1E) && bFieldXXFile ) || 
				(c == (char)0xC1 && bFieldXXFile)
			))
		{
			INCR_STREAM();
			continue;
		}

		switch(c)
		{
			case (char)0x29:
				two9 = true;
				break;

			case (char)0x2F:
				twoF = true;
				break;

			case (char)0x2A:
				twoA = true;
				break;

			case (char)0x2B:
				twoB = true;
				break;

			case (char)0xBA:
				speechBA = true;
				break;

			case (char)0x86:
				speech86 = true;
				break;

			case (char)0x2E:
				twoE = true;
				break;

			case (char)0xAF:
				speechAF = true;
				break;

			case (char)0xC1:
				fieldC1 = true;
				break;

			case (char)0x02:
				two6 = true;
				break;

			case (char)0x15:
				_150006 = true;
				break;

			case (char)0xCA:
				ca = true;
				break;

			case (char)0xB5:
				b5 = true;
				break;

			case (char)0xB7:
				b7 = true;
				break;

			case (char)0x00:
			case (char)0x94:
			case (char)0xC0:
			case (char)0x1E:
				zero6 = true;
				break;
		}

		//Non pointer needs to be skipped
		//CA 46 00 06 (Only in FIELD_06)
		if(ca)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			INCR_PEEK();
			if( *peek != (char)0x46 )
				goto notCA46;

			INCR_PEEK();
			if( *peek != 0 )
				goto notCA46;

			INCR_PEEK();
			if( *peek != (char)0x06 )
				goto notCA46;

			INCR_PEEK();
			inStream = peek;
			byteOffset += peekBytes;
			continue;

notCA46:
			{
				INCR_STREAM();
				continue;
			}
		}

		//		   00 01 02 03 04 05 06 07 08 08 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
		//Type21 = B7 03 06 PP PP 06 PP PP 07 PP PP 06 PP PP 06 PP PP 07 PP PP 06 PP PP 06 PP PP
		if(b7)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//00->01
			INCR_PEEK();
			if( *peek != (char)0x03 )
				goto b7Fail;

			//01->02
			INCR_PEEK();
			if( *peek != (char)0x06 )
				goto b7Fail;

			//02->03
			INCR_PEEK();
			
			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			newPointer.pointerType	= "Type21";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//03->04
			INCR_PEEK();

			//04->05
			INCR_PEEK();
			if( *peek != (char)0x06 )
				goto b7KindaFail;
			
			//05->06
			INCR_PEEK();
			
			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			newPointer.pointerType	= "Type21";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//06->07
			INCR_PEEK();

			//07->08
			INCR_PEEK();
			if( *peek != (char)0x07 )
				goto b7KindaFail;
			
			//08->09
			INCR_PEEK();
			
			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			newPointer.pointerType	= "Type21";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//09->10
			INCR_PEEK();

			//10->11
			INCR_PEEK();
			if( *peek != (char)0x06 )
				goto b7KindaFail;
			
			//11->12
			INCR_PEEK();
			
			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			newPointer.pointerType	= "Type21";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//12->13
			INCR_PEEK();

			//13->14
			INCR_PEEK();
			if( *peek != (char)0x06 )
				goto b7KindaFail;
			
			//14->15
			INCR_PEEK();
			
			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			newPointer.pointerType	= "Type21";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//15->16
			INCR_PEEK();

			//16->17
			INCR_PEEK();
			if( *peek != (char)0x07 )
				goto b7KindaFail;
			
			//17->18
			INCR_PEEK();
			
			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			newPointer.pointerType	= "Type21";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//18->19
			INCR_PEEK();

			//19->20
			INCR_PEEK();
			if( *peek != (char)0x06 )
				goto b7KindaFail;
			
			//20->21
			INCR_PEEK();
			
			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			newPointer.pointerType	= "Type21";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//21->22
			INCR_PEEK();

			//22->23
			INCR_PEEK();
			if( *peek != (char)0x06 )
				goto b7KindaFail;
			
			//23->24
			INCR_PEEK();
			
			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			newPointer.pointerType	= "Type21";
			outPointers.push_back(newPointer);
			peekBytes = 0;

b7KindaFail:
			{
				assert( 1 );
				return true;
			}

b7Fail:
			{
				INCR_STREAM();
				continue;
			}
		}

		//		   00 01 02 03 04
		//Type18 = B5 xx 00 06 PP PP
		if(b5)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;
			
			INCR_PEEK(); //00->01
			INCR_PEEK(); //01->02
			if( *peek != 0 )
				goto b5Fail;

			INCR_PEEK(); //02->03
			if( *peek != (char)0x06 )
				goto b5Fail;

			INCR_PEEK(); //03->04
	
			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			newPointer.pointerType	= "Type18";
			outPointers.push_back(newPointer);
			return true;

b5Fail:
			{
				INCR_STREAM();
				continue;
			}
		}

		//Only in SPEECH files
		//		    00 01 02 03 04
		//Type12  = 02 06 PP PP
		//Type12b = 02 xx 06 PP PP
		//Type12b = 02 xx 86 06 PP PP
		if(two6)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//00->01
			INCR_PEEK();
			if( *peek != (char)0x06 )
			{
				//Type12b

				//01->02
				INCR_PEEK();
			
				//Type12b
				if( *peek == (char)0x06 )
				{
					//02->03
					INCR_PEEK();
			
					//found the pointer
					newPointer.pointerStart = pointerStart;
					newPointer.pointer		= peek;
					newPointer.offset		= byteOffset + peekBytes;
					newPointer.pointerType	= "Type12b";
					outPointers.push_back(newPointer);
					return true;
				}

				//Type12c
				//02->03
				if( *peek == (char)0x86 )
				{
					INCR_PEEK();
			
					if( *peek == (char)0x06 )
					{
						//03->04
						INCR_PEEK();
				
						//found the pointer
						newPointer.pointerStart = pointerStart;
						newPointer.pointer		= peek;
						newPointer.offset		= byteOffset + peekBytes;
						newPointer.pointerType	= "Type12c";
						outPointers.push_back(newPointer);
						return true;
					}
				}

				INCR_STREAM();
				continue;
			}

			//01->02
			INCR_PEEK();

			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			newPointer.pointerType	= "Type12";
			outPointers.push_back(newPointer);
			return true;
		}

		//		   00 01 02 03 04 05 06 07 08 09
		//Type14 = 15 00 06 PP PP 06 PP PP
		//Type24 = 15 00 92 xx 94 06 PP PP			(START_01)
		//Type25 = 15 00 81 xx 8E 14 80 06 PP PP	(START_02)
		//Type26 = 15 00 92 xx 81 8E 14 80 06 PP PP (START_05)
		//Type29 = 15 00 xx xx 06 PP PP (SPEECH)
		//Type30 = 15 00 nn 06 PP PP  //nn is 92 or BF
		if(_150006)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//00->01
			INCR_PEEK();
			if( *peek != 0 )
			{
				INCR_STREAM();
				continue;
			}

			//01->02
			INCR_PEEK();
			if( *peek != (char)0x06 )
			{
				//Type24,25,26
				if( bStartFile || bSpeechFile || bDungeonFile)
				{
					if( *peek == (char)0x92 || *peek == (char)0xBF )
					{
						//02->03
						INCR_PEEK();
						if( *peek == (char)0x06 )
						{							
							INCR_PEEK();

							//found the pointer
							newPointer.pointerStart = pointerStart;
							newPointer.pointer		= peek;
							newPointer.offset		= byteOffset + peekBytes;
							newPointer.pointerType	= "Type30";
							outPointers.push_back(newPointer);
							return true;
						}
					//	if( *peek != (char)0x86 )
					//		goto failStart15;

						//03->04
						INCR_PEEK();
						
						//Type29
						if( bSpeechFile && *peek == (char)0x06 )
						{
							INCR_PEEK();

							//found the pointer
							newPointer.pointerStart = pointerStart;
							newPointer.pointer		= peek;
							newPointer.offset		= byteOffset + peekBytes;
							newPointer.pointerType	= "Type29";
							outPointers.push_back(newPointer);
							return true;
						}

						if( *peek != (char)0x94 ) //Type24
						{
							if(*peek != (char)0x81) //Type26
								goto failStart15;
						
							//04->05
							INCR_PEEK();
							if( *peek != (char)0x8E )
								goto failStart15;
							
							//05->06
							INCR_PEEK();
							if( *peek != (char)0x14 )
								goto failStart15;

							//06->07
							INCR_PEEK();
							if( *peek != (char)0x80 )
								goto failStart15;

							//07->08
							INCR_PEEK();
							if( *peek != (char)0x06 )
								goto failStart15;

							//08->09		
							INCR_PEEK();

							//found the pointer
							newPointer.pointerStart = pointerStart;
							newPointer.pointer		= peek;
							newPointer.offset		= byteOffset + peekBytes;
							newPointer.pointerType	= "Type26";
							outPointers.push_back(newPointer);
							return true;
					
							goto failStart15;
						}

						 //Type24

						//04->05
						INCR_PEEK();
						if( *peek != (char)0x06 )
							goto failStart15;

						//05->06
						INCR_PEEK();

						//found the pointer
						newPointer.pointerStart = pointerStart;
						newPointer.pointer		= peek;
						newPointer.offset		= byteOffset + peekBytes;
						newPointer.pointerType	= "Type24";
						outPointers.push_back(newPointer);
						return true;						
						
					}//if( *peek == 0x92
					else if( *peek == (char)0x81 )
					{
						//		   00 01 02 03 04 05 06 07 08
						//Type25 = 15 00 81 86 8E 14 80 06 PP PP	(START_05)
						
						//02->03
						INCR_PEEK();
						if( *peek != (char)0x86)
							goto failStart15;

						//03->04
						INCR_PEEK();
						if( *peek != (char)0x8E )
							goto failStart15;

						//04->05
						INCR_PEEK();
						if( *peek != (char)0x14 )
							goto failStart15;

						//05->06
						INCR_PEEK();
						if( *peek != (char)0x80 )
							goto failStart15;

						//06->07
						INCR_PEEK();
						if( *peek != (char)0x06 )
							goto failStart15;

						INCR_PEEK();
						
						//found the pointer
						newPointer.pointerStart = pointerStart;
						newPointer.pointer		= peek;
						newPointer.offset		= byteOffset + peekBytes;
						newPointer.pointerType	= "Type25";
						outPointers.push_back(newPointer);
						return true;
					}
				}

failStart15:
				INCR_STREAM();
				continue;
			}

			//02->03
			INCR_PEEK();

			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			newPointer.pointerType	= "Type14";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//03->04
			INCR_PEEK();

			//04->05
			INCR_PEEK();
			if( *peek == (char)0x06 )				
			{
				//05->06
				INCR_PEEK();

				//found the pointer
				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes - 1;
				newPointer.pointerType	= "Type14e";
				outPointers.push_back(newPointer);
			}

			return true;
		}

		//			00 01 02
		//Type11 =  ss 06 PP PP //ss = 0, in FIELD_XX can be 94, C0, 1E
		if(zero6)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//00->01
			INCR_PEEK();

			//Type11
			if( *peek != (char)0x06 )
			{
				INCR_STREAM();
				continue;
			}

			//01->02
			INCR_PEEK();

			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			newPointer.pointerType	= "Type11";
			outPointers.push_back(newPointer);
			return true;
		}

		//		   00 01 02 03 04 05
		//Type10 = C1 03 00 00 06 PP PP
		if(fieldC1)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//00->01
			INCR_PEEK();
			if( *peek != (char)0x03 )
				goto failFieldC1;

			//01->02
			INCR_PEEK();
			if( *peek != 0 )
				goto failFieldC1;

			//02->03
			INCR_PEEK();
			if( *peek != 0 )
				goto failFieldC1;

			//03->04
			INCR_PEEK();
			if( *peek != (char)0x06 )
				goto failFieldC1;

			//04->05
			INCR_PEEK();

			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			newPointer.pointerType	= "Type10";
			outPointers.push_back(newPointer);
			return true;

failFieldC1:
			{
				INCR_STREAM();
				continue;
			}
		}

		//		  00 01 02 03 04 05 06 07
		//Type9 = AF xx 07 PP PP nn 07 PP PP //nn is 88 or 86
		if(speechAF)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//00->01
			INCR_PEEK();

			//01->02
			INCR_PEEK();
			if( *peek != (char)0x07 )
			{
				INCR_STREAM();
				continue;
			}

			//02->03
			INCR_PEEK();

			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			newPointer.pointerType	= "Type9";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//03->05
			INCR_PEEK();
			INCR_PEEK();

			int typeB = *peek == (char)0x86;
			if( !(*peek == (char)0x86 || *peek == (char)0x88) )
				return true;

			//05->06
			INCR_PEEK();
			if( *peek != (char)0x07 )
				return true;

			//06->07
			INCR_PEEK();
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			newPointer.pointerType	= typeB ? "Type9b" : "Type9e";
			outPointers.push_back(newPointer);

			return true;

		}

		//Special pointer only found in SPEECH files
		//		  00 01 02 03 04 05
		//Type6 = BA xx xx 00 07 PP PP				//Only in SPEECH files
		if(speechBA)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//00->01
			INCR_PEEK();

			//01->02
			INCR_PEEK();

			//02->03
			INCR_PEEK();
			if(*peek != 0)
			{
				INCR_STREAM();
				continue;
			}

			//03->04
			INCR_PEEK();
			if( *peek != (char)0x07 )
			{
				INCR_STREAM();
				continue;
			}

			//04->05
			INCR_PEEK();

			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			newPointer.pointerType	= "Type6";
			outPointers.push_back(newPointer);
			return true;
		}

		//		  00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25
		//Type7 = 86 B6 01 01 01 06 PP PP 07 PP PP 06 PP PP 06 PP PP 07 PP PP 06 PP PP 06 PP PP //Only in SPEECH files
		if(speech86)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//00->01
			INCR_PEEK();
			if( *peek != (char)0xB6 )
				goto failSpeech86;

			//01->02
			INCR_PEEK();
			if( *peek != (char)0x01 )
				goto failSpeech86;

			//02->03
			INCR_PEEK();
			if( *peek != (char)0x01 )
				goto failSpeech86;

			//01->04
			INCR_PEEK();
			if( *peek != (char)0x01 )
				goto failSpeech86;

			//04->05
			INCR_PEEK();
			if( *peek != (char)0x06 )
				goto failSpeech86;

			//05->06
			INCR_PEEK(); //found a pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			outPointers.push_back(newPointer);
			peekBytes = 0;		
			
			//06->07
			INCR_PEEK();
			
			//07->08
			INCR_PEEK();
			assert(*peek == (char)0x07 );

			//08->09
			INCR_PEEK(); //found a pointer			
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			newPointer.pointerType	= "Type7";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//09->10
			INCR_PEEK();

			//10->11
			INCR_PEEK();
			assert(*peek == (char)0x06 );

			//11->12
			INCR_PEEK(); //found a pointer			
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//12->13
			INCR_PEEK();

			//13->14
			INCR_PEEK();
			assert(*peek == (char)0x06 );

			//14->15
			INCR_PEEK(); //found a pointer			
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			outPointers.push_back(newPointer);
			peekBytes = 0;
		
			//15->16
			INCR_PEEK();

			//16->17
			INCR_PEEK();
			assert(*peek == (char)0x07 );

			//17->18
			INCR_PEEK(); //found a pointer			
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			newPointer.pointerType	= "Type7e";
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//18->19
			INCR_PEEK();

			//19->20
			INCR_PEEK();
			assert(*peek == (char)0x06 );

			//20->21
			INCR_PEEK(); //found a pointer			
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			outPointers.push_back(newPointer);
			peekBytes = 0;

			//21->22
			INCR_PEEK();

			//22->23
			INCR_PEEK();
			assert(*peek == (char)0x06 );

			//24->25
			INCR_PEEK(); //found a pointer			
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= peekBytes - 1;
			outPointers.push_back(newPointer);
			peekBytes = 0;

			return true;
failSpeech86:
			{
				INCR_STREAM();
				continue;
			}
		}

		//			00 01 02 03 04 05 06 07 08 09 10 11 12 13
		//Type19 =  2F 14 10 04 00 00 00 06 PP PP 06 PP PP	(Dungeon)
		//Type19b = 2F 10 25 00 00 81 90 03 00 80 06 6B 00 06 55 01 )	
		//Type20 =  29 14 10 02 00 00 00 PP PP 			(Dungeon)	
		if( (twoF || two9) && bDungeonFile)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//00->01
			INCR_PEEK();
			if( *peek == (char)0x14 )
			{
				//01->02
				INCR_PEEK();
				if( *peek != (char)0x10 )
				{
					goto _2f1410Fail;
				}

				//02->03
				INCR_PEEK();
				if( two9 )
				{
					if( *peek == (char)0x02 )
					{
						//03->04
						INCR_PEEK();
						if( *peek != 0 )
							goto _2f1410Fail;

						//04->05
						INCR_PEEK();
						if( *peek != 0 )
							goto _2f1410Fail;

						//05->06
						INCR_PEEK();
						if( *peek != 0 )
							goto _2f1410Fail;

						//06->07
						INCR_PEEK();

						//found a pointer
						newPointer.pointerStart = pointerStart;
						newPointer.pointer		= peek;
						newPointer.offset		= peekBytes + byteOffset;
						newPointer.pointerType	= "Type20";
						outPointers.push_back(newPointer);

						return true;
					}
					else
					{
						goto _2f1410Fail;
					}
				}
				else if(twoF)
				{
					//			00 01 02 03 04 05 06 07 08 09 10 11
					//Type19 =  2F 14 10 04 00 00 00 06 PP PP 06 PP PP	(Dungeon)
					if( *peek == (char)0x04 )
					{
						//03->04
						INCR_PEEK();
						if( *peek != 0 )
							goto _2f1410Fail;

						//04->05
						INCR_PEEK();
						if( *peek != 0 )
							goto _2f1410Fail;

						//05->06
						INCR_PEEK();
						if( *peek != 0 )
							goto _2f1410Fail;

						//06->07
						INCR_PEEK();
						if( *peek != (char)0x06 )
							goto _2f1410Fail;

						//07->08
						INCR_PEEK();

						//found a pointer
						newPointer.pointerStart = pointerStart;
						newPointer.pointer		= peek;
						newPointer.offset		= peekBytes + byteOffset;
						newPointer.pointerType	= "Type19";
						outPointers.push_back(newPointer);
						peekBytes = 0;

						//08->09
						INCR_PEEK();

						//09->10
						INCR_PEEK();
						if( *peek == (char)0x06 )
						{
							//10->11
							INCR_PEEK();

							newPointer.pointerStart = pointerStart;
							newPointer.pointer		= peek;
							newPointer.offset		= peekBytes - 1;
							newPointer.pointerType	= "Type19e";
							outPointers.push_back(newPointer);
						}

						return true;

					}
					else
						goto _2f1410Fail;
				}
				
_2f1410Fail:
				{
					INCR_STREAM();
					continue;
				}
			}				
		}

		//00->01
		INCR_STREAM();

		//Second byte is not that of a pointer, so just continue
		if( *inStream != (char)0x10 )
		{
			continue;
		}

		//01->02
		INCR_STREAM();		

		//	       00 01 02 03 04 05 06 07 08
		//Type8  = 2E 10 00 00 00 PP PP
		//Type8b = 2E 10 01 00 00 4E 00 07 PP PP
		if(twoE)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//02
			if( *peek == (char)0x01 )
			{
				//02->03
				INCR_PEEK();
				if( *peek != 0)
					goto failTwoE;

				//03->04
				INCR_PEEK();
				if( *peek != 0 )
					goto failTwoE;

				//04->05
				INCR_PEEK();
				if( *peek != (char)0x4E )
					goto failTwoE;

				//05->06
				INCR_PEEK();
				if( *peek != 0 )
					goto failTwoE;

				//06->07
				INCR_PEEK();
				if( *peek != (char)0x07 )
					goto failTwoE;

				//07->08
				INCR_PEEK();
				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= byteOffset + peekBytes;
				newPointer.pointerType	= "Type8b";
				outPointers.push_back(newPointer);
				return true;
			}
			else if( *peek != 0 )
				goto failTwoE;

			//02->03
			INCR_PEEK();
			if(*peek != 0)
				goto failTwoE;

			//03->04
			INCR_PEEK();
			if(*peek != 0)
				goto failTwoE;

			//found a pointer
			INCR_PEEK(); 
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			newPointer.pointerType	= "Type8";
			outPointers.push_back(newPointer);
			return true;

failTwoE:
			{
				continue;
			}
		}

		//			00 01 02 03 04 05 06 07 08 09 10 11 12
		//Type3  =  2F 10 00 00 00 87 00 xx 06 PP PP kk PP PP //kk is 06 or 07
		//Type4  =  2F 10 00 00 00 87 00 06 PP PP 07 PP PP 
		//Type13 =  2F 10 xx 00 00 SS PP PP where SS is 88, C6
		//Type16 =  2F 10 02 00 00 xx 80 07 pp pp  (found in Start so far)
		//Type22 =  2F 10 01 00 00 xx 06 PP PP  (Speech)
		//Type23 =  2F 10 01 00 00 06 PP PP 06 PP PP(Speech)
		//Type31  = 2F 10 xx 00 00 xx xx xx 00 xx 06 PP PP 06 PP PP
		if(twoF)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;
			char secondByte = *peek;
			
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

			//Type13
			if( 0)// *peek == (char)0x88 || *peek == (char)0xC6)
			{
				//05->06
				INCR_PEEK();

				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes + byteOffset;
				newPointer.pointerType	= "Type13";
				outPointers.push_back(newPointer);

				return true;
			}
			
			//05
			if( *peek != (char)0x87 )
			{
				//Type16
				if(bStartFile)
				{
					BytesList::iterator peekOrig = peek;
					int peekBytesOrig = peekBytes;

					//05->06
					INCR_PEEK();
					if( *peek != (char)0x80 )
						goto type16Fail;

					//06->07
					INCR_PEEK();
					if( *peek != (char)0x07 )
						goto twoFFail;

					//07->08
					INCR_PEEK();

					//found pointer
					newPointer.pointerStart = pointerStart;
					newPointer.pointer		= peek;
					newPointer.offset		= peekBytes + byteOffset;
					newPointer.pointerType	= "Type16";
					outPointers.push_back(newPointer);
					return true;

type16Fail:
					peek = peekOrig;
					peekBytes = peekBytesOrig;
				}

				//			00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15
				//Type22 =  2F 10 01 00 00 xx 06 PP PP  (Speech) 
				//Type23 =  2F 10 01 00 00 06 PP PP 06 PP PP(Speech)
				//Type31  = 2F 10 xx 00 00 xx xx xx 00 xx 06 PP PP 06 PP PP
				//else if(bSpeechFile && secondByte == (char)0x01)
				if(bDungeonFile || bSpeechFile || bStartFile)
				{
					BytesList::iterator peekOrig = peek;
					int peekBytesOrig = peekBytes;

					if( *peek == (char)0x06 )
					{
						//05->06
						INCR_PEEK();

						//found pointer
						newPointer.pointerStart = pointerStart;
						newPointer.pointer		= peek;
						newPointer.offset		= peekBytes + byteOffset;
						newPointer.pointerType	= "Type23";
						outPointers.push_back(newPointer);
						peekBytes = 0;

						//06->07
						INCR_PEEK();

						//07->08
						INCR_PEEK();
						if( *peek == (char)0x06 )
						{
							//08->09
							INCR_PEEK();
						
							//found pointer
							newPointer.pointerStart = pointerStart;
							newPointer.pointer		= peek;
							newPointer.offset		= peekBytes - 1;
							newPointer.pointerType	= "Type23e";
							outPointers.push_back(newPointer);
						}

						return true;
					}

					//05->06
					INCR_PEEK();
					if( *peek != (char)0x06 )
					{
						//			00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15
						//Type31  = 2F 10 xx 00 00 xx xx xx 00 xx 06 PP PP 06 PP PP

						//06->07
						INCR_PEEK();

						//07->08
						INCR_PEEK();
						if( *peek != (char)0 )
							goto type22Fail;

						//08->09
						INCR_PEEK();

						//09->10
						INCR_PEEK();
						if( *peek != (char)0x06 )
							goto type22Fail;

						//10->11
						INCR_PEEK();
					
						//found pointer
						newPointer.pointerStart = pointerStart;
						newPointer.pointer		= peek;
						newPointer.offset		= peekBytes + byteOffset;
						newPointer.pointerType	= "Type31";
						outPointers.push_back(newPointer);
						peekBytes = 0;

						//11->12
						INCR_PEEK();

						//12->13
						INCR_PEEK();

						if( *peek != (char)0x06 )
							return true;

						//13->14
						INCR_PEEK();

						//found pointer
						newPointer.pointerStart = pointerStart;
						newPointer.pointer		= peek;
						newPointer.offset		= peekBytes - 1;
						newPointer.pointerType	= "Type31b";
						outPointers.push_back(newPointer);

						return true;
					}

					if( bSpeechFile )
					{
						//06->07
						INCR_PEEK();

						//found pointer
						newPointer.pointerStart = pointerStart;
						newPointer.pointer		= peek;
						newPointer.offset		= peekBytes + byteOffset;
						newPointer.pointerType	= "Type22";
						outPointers.push_back(newPointer);
						return true;
					}
type22Fail:
					peek = peekOrig;
					peekBytes = peekBytesOrig;
				}
			}
			else
			{
				int k = 0;
				++k;
			}

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
				newPointer.pointerType	= "Type4";
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
					newPointer.pointerType	= "Type4e";
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
			newPointer.pointerType	= "Type3";
			outPointers.push_back(newPointer);

			//reset peek counter for next pointer
			peekBytes = 0;

			//09->10
			INCR_PEEK();

			//10->11
			INCR_PEEK();
			if( *peek == (char)(0x06) || *peek == (char)0x07 )
			{
				INCR_PEEK();
			
				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
				newPointer.pointerType	= "Type3e";
				outPointers.push_back(newPointer);
			}

			return true;

twoFFail:
			{}
		}
		
		//If this is a Type1 pointer
		//			00 01 02 03 04 05 06 07 08 09 10 11 12 13 14
		//Type1  =	29 10 xx 00 00 PP PP xx xx 06 PP PP
		//Type2  =  Type1 +				 80 07 PP PP
		//Type5  =	LL 10 xx 00 25 00 PP PP	06 PP PP		//LL (29, 2A, 2B) nn between 01 and 09
		//Type15 =  LL 10 xx 00 00 PP PP 06 PP PP xx 06 PP PP 
		//Type17 =  29 10 xx 00 00 PP PP 07 PP PP
		//Type27 =  29 10 00 00 00 PP PP B6 01 02 01 06 PP PP 06 PP PP (Dungeon and Speech)
		//Type
		//02
		if( bFirstPointerFound && (two9 || twoA || twoB) )// && (*inStream >= 0x00 && *inStream <= 0xFF) )//0x09) )
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//02->03
			INCR_PEEK();
			if(*peek != 0)
				continue;

			//03->04
			INCR_PEEK();			
			if( *peek != 0)
			{
				//Perhaps this is a Type5
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
					newPointer.pointerType	= "Type5";
					outPointers.push_back(newPointer);
					peekBytes = 0;
					
					INCR_PEEK(); //06->07
					INCR_PEEK(); //07->08
					if( *peek != (char)0x06 )
						return true;

					//08->09
					INCR_PEEK(); 

					newPointer.pointerStart = pointerStart;
					newPointer.pointer		= peek;
					newPointer.offset		= peekBytes - 1;
					newPointer.pointerType	= "Type5e";
					outPointers.push_back(newPointer);

					return true;
					
type5Fail:
					{}
				}

				continue;
			}

			//04->05
			INCR_PEEK();

			//found the pointer
			newPointer.pointerStart = pointerStart;
			newPointer.pointer		= peek;
			newPointer.offset		= byteOffset + peekBytes;
			newPointer.pointerType	= "Type1";
			outPointers.push_back(newPointer);
			peekBytes = 0;
			
			INCR_PEEK(); //5->6
			INCR_PEEK(); //6->7
			
			//Type2
			if( *peek == (char)0x80 )
			{
				BytesList::iterator origPeek = peek;

				//7->8
				INCR_PEEK();
				if( *peek == (char)0x07 )
				{
					INCR_PEEK();

					newPointer.pointerStart = pointerStart;
					newPointer.pointer		= peek;
					newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
					newPointer.pointerType	= "Type2";
					outPointers.push_back(newPointer);

					return true;
				}

				//go back one
				peek = origPeek;
				peekBytes--;
			}
			//Type27
			else if( *peek == (char)0xB6 )
			{
				BytesList::iterator origPeek = peek;
				int origPeekBytes = peekBytes;

				//			00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15
				//Type27 =  29 10 00 00 00 PP PP B6 01 02 01 06 PP PP 06 PP PP (Dungeon and Speech)
				
				//07->08
				INCR_PEEK();
				if( *peek != (char)0x01 )
					goto type27Fail;

				//08->09
				INCR_PEEK();
				if( *peek != (char)0x02 )
					goto type27Fail;

				//09->10
				INCR_PEEK();
				if( *peek != (char)0x01 )
					goto type27Fail;

				//10->11
				INCR_PEEK();
				if( *peek != (char)0x06 )
					goto type27Fail;

				//11->12
				INCR_PEEK();

				//found a pointer
				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
				newPointer.pointerType	= "Type27";
				outPointers.push_back(newPointer);
				peekBytes = 0;
				
				INCR_PEEK(); //12->13
				INCR_PEEK(); //13->14
				if( *peek != (char)0x06 )
					return true;

				//14->15
				INCR_PEEK();

				//found a pointer
				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
				newPointer.pointerType	= "Type27e";
				outPointers.push_back(newPointer);
				return true;

type27Fail:
				{
					peek = origPeek;
					peekBytes = origPeekBytes;
				}

			}

			//			00 01 02 03 04 05 06 07 08 09 10 11 12
			//Type27b = 29 10 00 00 00 PP PP 9F xx xx 06 PP PP
			else if( *peek == (char)0x9F )
			{
				BytesList::iterator origPeek = peek;
				int origPeekBytes = peekBytes;

				//07->08
				INCR_PEEK();

				//08->09
				INCR_PEEK();

				//09->10
				INCR_PEEK();
				if( *peek != (char)0x06 )
					goto type27bFail;

				//10->11
				INCR_PEEK();

				//found a pointer
				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
				newPointer.pointerType	= "Type27b";
				outPointers.push_back(newPointer);
				return true;

type27bFail:
				{
					peek = origPeek;
					peekBytes = origPeekBytes;
				}
				
			}
			
			//			00 01 02 03 04 05 06 07 08 09 10 11 12
			//Type15 =  LL 10 xx 00 00 PP PP 06 PP PP xx 06 PP PP 
			if( *peek == (char)0x06 )
			{
				INCR_PEEK(); //7->8
				
				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
				newPointer.pointerType	= "Type15";
				outPointers.push_back(newPointer);
				peekBytes = 0;

				INCR_PEEK(); //8->9
				INCR_PEEK(); //9->10
				INCR_PEEK(); //10->11

				if( *peek == (char)0x06 )
				{
					INCR_PEEK();
					newPointer.pointerStart = pointerStart;
					newPointer.pointer		= peek;
					newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
					newPointer.pointerType	= "Type15e";
					outPointers.push_back(newPointer);
				}

				return true;
			}
			
			//Type17
			if( *peek == (char)0x07 )
			{
				INCR_PEEK(); //7->8
				
				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
				newPointer.pointerType	= "Type17";
				outPointers.push_back(newPointer);

				return true;
			}
			

			//Type1E
		//	00 01 02 03 04 05 06 07 08 09
		//	29 10 00 00 00 PP PP kk xx 06 PP PP  //kk = 9A or B7(dungeon&speech files only)
			if( !(*peek == (char)0x9A || *peek == (char)0x9F ) )
				return true;

			INCR_PEEK(); //7->8			

			INCR_PEEK(); //8->9
			if(*peek == (char)0x06)
			{
				INCR_PEEK(); //10->11

				newPointer.pointerStart = pointerStart;
				newPointer.pointer		= peek;
				newPointer.offset		= peekBytes - 1; //-1 because the fixup does a +1 to skip past second byte
				newPointer.pointerType	= "Type1e";
				outPointers.push_back(newPointer);
			}

			return true;
		}
//		INCR_STREAM();

		if( *inStream != 0x00 )
		{
			continue;
		}

		continue;
	}

	return false;
}

short GetPointerOffset(const OSIVector &newStringsInfo, const OSIVector &origStringsInfo, unsigned short currByte)
{
	short offset = 0;
	for(size_t i = 0; i < newStringsInfo.size(); ++i)
	{
		const OriginalStringInfo &newStringInfo = newStringsInfo[i];

		if((unsigned short)newStringInfo.address < currByte)
		{
			const OriginalStringInfo &origStringInfo = origStringsInfo[i];

			offset += newStringInfo.numBytes - origStringInfo.numBytes;
		}
	}

	return offset;
}

static vector<int> GFoundAddresses;
void FindPotentialDuplicatePointers(const OrigAddressInfo &pointerInfo, const BytesList &fileBytes, const vector<OrigAddressInfo> &otherPointers, FILE *pPossiblePointersLogFile)
{
#define INCR_BYTE() ++bytesIter; ++currByte;

	if(pointerInfo.firstByte == 0 && pointerInfo.secondByte == 0)
		return;

	const int address = (pointerInfo.firstByte << 8) | (pointerInfo.secondByte & 0xff);
	for(size_t i = 0; i < GFoundAddresses.size(); ++i)
	{
		if( GFoundAddresses[i] == address )
			return;
	}

	int currByte = 0;
	bool bStringStarted1	= false;
	bool bStringStarted2	= false;
	bool bStringEndStarted	= false;
	for( BytesList::const_iterator bytesIter = fileBytes.begin(); bytesIter != fileBytes.end(); ++bytesIter, ++currByte)
	{
		//Skip the passed in pointer
		if(currByte == pointerInfo.newLoc)
		{
			bStringStarted1 = bStringStarted2 = false;
			INCR_BYTE();
			continue;
		}

		//***Make sure we aren't in between a string***
		const int byteValue = (int) ((unsigned char)*bytesIter);

		//See if we are at the start of a string
		if(!bStringStarted2 && (byteValue == startText1 || byteValue == startText1_b) )
		{
			bStringStarted1 = true;
		}
		//See if this is the second start flag
		else if(bStringStarted1 && !bStringStarted2 && (byteValue == (int)IndentChar || byteValue == (int)'(' ) )
		{
			bStringStarted2 = true;

			//start of a string so skip
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
			if(byteValue == endText1)
			{
				bStringEndStarted = true;
			}
			else if(byteValue == endText2)
			{
				bStringStarted1 = bStringStarted2 = false;
				continue;
			}
			else //This byte is part of a string
			{
				continue;
			}
		}
		//***Done checking if in middle of a string***

		//		 00 01 02 03 04
		//Ignore 15 00 88 bb bb
		if( *bytesIter == (char)0x15 )
		{
			BytesList::const_iterator peek = bytesIter;
			++peek;

			if( *peek == 0)
			{
				++peek;
				if( *peek == (char)0x88 )
				{
					INCR_BYTE(); //00->01
					INCR_BYTE(); //01->02
					INCR_BYTE(); //02->03
					INCR_BYTE(); //03->04
					continue;
				}
			}
		}

		//First byte found
		if( *bytesIter == (char)pointerInfo.secondByte )
		{
			//Go to next byte
			INCR_BYTE();

			//Make sure we are not at the end of the file
			if( bytesIter == fileBytes.end() )
				break;

			//See if it matches the second byte
			if( *bytesIter == (char)pointerInfo.firstByte  )
			{
				//Make sure this dup hasn't already been found
				bool bAlreadyFound = false;
				const int currPointerAddress = currByte-1;
				for(size_t otherPointerIndex = 0; otherPointerIndex < otherPointers.size(); ++otherPointerIndex)
				{
					int diffBetweenLocations = otherPointers[otherPointerIndex].newLoc - currPointerAddress;
					if(diffBetweenLocations < 0)
						diffBetweenLocations *= -1;

					if( diffBetweenLocations < 2 )
					{
						bAlreadyFound = true;
						break;
					}
				}

				//We already know about this dup
				if(bAlreadyFound)
					continue;

				//Real duplicate
				fprintf(pPossiblePointersLogFile, "CurrValue: %.2X %.2X  FixedValue: %.2X %.2X     @%.4X \n", pointerInfo.secondByte, pointerInfo.firstByte, pointerInfo.newSecondByte, pointerInfo.newFirstByte, currByte-1);

				//Store where this duplicate is so we don't print out the same one twice
				GFoundAddresses.push_back( (pointerInfo.firstByte << 8) | (pointerInfo.secondByte & 0xff) );

				bStringStarted1 = bStringStarted2 = false;
			}
		}
	}
}

//Inserts English text into an eve file
void InsertEnglishText()
{
	//Kana lookup table
	unsigned char translationTable[512];
	FILE *pIndexFile = NULL;
	fopen_s(&pIndexFile, "DF2Files\\Misc\\SPE_MAIN.bin", "rb");
	assert(pIndexFile);

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
	const string pointerLogExtension("_PointerLog.txt");
	const string textLogExtension("_TextLog.txt");
	const string possiblePointersLogExtension("_PossiblePointers.txt");

	//create log directories
	struct tm timeInfo;
	time_t rawtime;
	time ( &rawtime );
	localtime_s(&timeInfo, &rawtime );
	char dirBuffer[512];
	sprintf_s(dirBuffer, 512, "%sLogs_%i_%i_%i_%i\\", df2LogPath.c_str(), timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);	
	const string logDir(dirBuffer);

	//base dir
	int dirCreated = _mkdir(dirBuffer);
	assert( dirCreated == 0);

	//other dir
	dirCreated = sprintf_s(dirBuffer, 512, "%s\\%s", logDir.c_str(), df2PointerLogPath.c_str());
	dirCreated = _mkdir(dirBuffer);
	assert( dirCreated == 0);

	dirCreated = sprintf_s(dirBuffer, 512, "%s\\%s", logDir.c_str(), df2TextInsertionLogPath.c_str());
	dirCreated = _mkdir(dirBuffer);
	assert( dirCreated == 0);

	dirCreated = sprintf_s(dirBuffer, 512, "%s\\%s", logDir.c_str(), df2PossiblePointersLogPath.c_str());
	dirCreated = _mkdir(dirBuffer);
	assert( dirCreated == 0);

	for(size_t currFile = 0; currFile < eveFiles.size(); ++currFile)
	{
		//Open file with translated text, if it doesn't exist, then skip translating the eve file
		const string engFileName		= df2EnglishPath + eveFiles[currFile] + engExtension;
		FILE *pInEngFile				= NULL;
		fopen_s(&pInEngFile, engFileName.c_str(), "rb");
		if(!pInEngFile)
			continue;

		GFoundAddresses.clear();

		fprintf(stdout, "Processing %s\n", eveFiles[currFile].c_str());

		const string eveFileName					= df2EveFilesPath	+ eveFiles[currFile] + eveExtension;
		const string translatedFileName				= df2TranslatedPath + eveFiles[currFile] + eveExtension;
		const string pointerLogFileName				= logDir + df2PointerLogPath + eveFiles[currFile] + pointerLogExtension;
		const string textInsertLogFileName			= logDir + df2TextInsertionLogPath + eveFiles[currFile] + textLogExtension;
		const string possiblePointersLogFileName	= logDir + df2PossiblePointersLogPath + eveFiles[currFile] + possiblePointersLogExtension;
		FILE *pInEveFile							= NULL;
		FILE *pOutEveFile							= NULL;
		FILE *pPointerLogFile						= NULL;
		FILE *pTextInsertLogFile					= NULL;
		FILE *pPossiblePointersLogFile				= NULL;
		
		fopen_s(&pInEveFile,			eveFileName.c_str(), "rb");
		fopen_s(&pOutEveFile,			translatedFileName.c_str(), "wb");
		fopen_s(&pPointerLogFile,		pointerLogFileName.c_str(), "w");
		fopen_s(&pTextInsertLogFile,	textInsertLogFileName.c_str(), "w");
		fopen_s(&pPossiblePointersLogFile, possiblePointersLogFileName.c_str(), "w");

		assert(pInEveFile);
		assert(pOutEveFile);
		assert(pPointerLogFile);
		assert(pTextInsertLogFile);
		assert(pPossiblePointersLogFile);

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
		{
			bFirstPointerFound = true; //Only need to search for it in START FILES
			bStartFile = false;
		}
		else
		{
			bFirstPointerFound = true;//false; //For all other files we don't look for it
			bStartFile = true;
		}

		if( eveFileName.find("SPEECH") == string::npos )
			bSpeechFile = false;
		else
			bSpeechFile = true;

		if(eveFileName.find("FIELD_") == string::npos)
			bFieldXXFile = false;
		else
			bFieldXXFile = true;

		if(eveFileName.find("DUNGEON") == string::npos)
			bDungeonFile = false;
		else
			bDungeonFile = true;

		while(currByte != EOF)
		{
			currByte = fgetc(pInEveFile);
			if(currByte == EOF)
				break;

			++byteCount;

			//See if we are at the start of a string
			if(!bStringStarted2 && (currByte == startText1 || currByte == startText1_b) )
			{
				bStringStarted1 = true;
			}
			//See if this is the second start flag
			else if(bStringStarted1 && !bStringStarted2 && (currByte == startText2 || currByte == startText2_b || currByte == startText2_c || (currByte == startText2_d && bFieldXXFile) || currByte == startText2_e || currByte == startText2_f || currByte == startText2_g) )
			{
				currStringInfo.numBytes = 0;

				//The string will start at the next byte
				bStringStarted2 = true;

				bool bSpecialCase = (currByte == startText2_b) || (currByte == startText2_c || currByte == startText2_d || currByte == startText2_e || currByte == startText2_f || currByte == startText2_g);
				if(!bSpecialCase)
					fileBytes.push_back(IndentChar);

				assert(stringInsertionPoints.size() == origStringsInfo.size());

				//Keep track of where this string used to be so we can insert our translated text into it
				StringInsertionLocation locInfo;
				
				if(bSpecialCase)
				{
					currStringInfo.numBytes = 1;				//This byte counts as being part of the string
					locInfo.address			= byteCount-1;		//Insertion should start before this byte, meaning right after 85					
				}
				else
				{
					locInfo.address			= byteCount;
				}

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
				else if(currByte == endText2)// && bStringEndStarted)
				{
					//if being end marker appeared (0x15), insert that in
					if(bStringEndStarted)
					{
						fileBytes.push_back( (char)endText1 );
					}
	
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
					//fileBytes.push_back( (char)endText1 );
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

			//insertion itertator
			list<char>::iterator insertionPoint = stringInsertionPoints[insertPoint].insertionPoint;

			//insertion iterator starts at the A0 in 85A0 and 85 in special case
			bool bSpecialCase = *(insertionPoint) != (char)IndentChar;
			insertionPoint++;

			newStringInfo.numBytes = 0;
			newStringInfo.address = stringInsertionPoints[insertPoint].address;

			//Insert the translated string
			int stringsPrinted = bSpecialCase ? 0 : 1; //already have an indent
			int totalPrinted = bSpecialCase ? 0 : 1;
			int numLines = 0;
			int wordStartIndex = -1;
			list<char>::iterator wordStartInsertionPoint;

			char		tmp[1024];
			int			tmpCount = 0;
			const int	maxPrint = 36 + 36 + 35;
			int			maxPrintForLine = 36;

			memset(tmp, 0, sizeof(tmp));
			bool bAutoWrapping = false;
			for(size_t i = 0; i < strLen; ++i)
			{
				if( totalPrinted == maxPrint )
					break;

				assert(totalPrinted < maxPrint);

				const char currLetter = inEnglishStrBuffer[i];

				//discard new lines and what not
				if(currLetter < 0x20 || (currLetter == ' ' && (stringsPrinted == 0 && !bAutoWrapping)) )
					continue;

				//insert our new string
				if( !(currLetter == ' ' && (stringsPrinted + 1 > maxPrintForLine)) )
				{
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

					if( totalPrinted == maxPrint )
						break;
				}
				else
					wordStartIndex = -1;

				bAutoWrapping = false;

				if(++stringsPrinted > maxPrintForLine)
				{
					if(++numLines == 3)
						break;

					if(numLines == 2)
						maxPrintForLine = 35;

					stringsPrinted = 0;

					//If the 37th character is a space, just remove the space and let the game autowrap
					if( wordStartIndex > -1 && tmp[wordStartIndex] == ' ' && (i - wordStartIndex) == 0 )
					{
						bAutoWrapping = true;
						/*
						//go back and delete the space
						list<char>::iterator reverseIter = wordStartInsertionPoint;
						reverseIter--;
						fileBytes.erase(reverseIter);

						//fix up our temp array
						memcpy(tmp + wordStartIndex, tmp + wordStartIndex + 1, 36);
						tmpCount--;

						//remove space count
						newStringInfo.numBytes--;
						totalPrinted--;	*/					
					}
					else
					{
						//insert new line
						if( wordStartIndex > -1 )
						{
							fileBytes.insert(wordStartInsertionPoint, 0x0D);
							memcpy(tmp + wordStartIndex + 1, tmp + wordStartIndex, 36);
							tmp[wordStartIndex] = 0x0D;
							tmpCount++;
							stringsPrinted += 1 + ((int)i - wordStartIndex);
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

		int db = 1;
		for( BytesList::iterator bytesIter = fileBytes.begin(); bytesIter != fileBytes.end(); ++bytesIter, ++currByte)
		{
			PointerVector outPointers;

			if( GetNextPointer(bytesIter, fileBytes.end(), outPointers) == false)
				break;

			if(db == 66)
			{
				int k = 0;
				++k;
			}
			++db;

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
				unsigned short address      = (firstByte << 8) | (secondByte & 0xff);

				short offset = GetPointerOffset(newStringsInfo, origStringsInfo, address);

				int overflowCheck = address;
				if(address + offset > 65536)
				{
					fprintf(stdout, "WARNING: Address is out of 16 byte range: %.4X %.4X ", address, offset );
					fprintf(pPointerLogFile, "WARNING: Address is out of 16 byte range: %.4X %.4X ", address, offset );
				}

				address += offset;

				char orgFirstByte	= firstByte;
				char orgSecondByte	= secondByte; 
				firstByte			= address >> 8;
				secondByte			= address & 0xff;

				logInfo.push_back( OrigAddressInfo(orgFirstByte, orgSecondByte, firstByte, secondByte, currByte-1, origBytes, outPointers[i].pointerStart, 
									outPointers[i].pointer, outPointers[i].pointerType) );
			}			
		}

		//Save pointer log file
		fprintf(pPointerLogFile, "Little Endian           Big Endian		NewValue						Full									Type\n");
		for(size_t i = 0; i < logInfo.size(); ++i)
		{
			OrigAddressInfo &currInfo = logInfo[i];
			
			if( ((currInfo.newFirstByte << 8) | (currInfo.newSecondByte & 0xff)) >= fileBytes.size() )
			{
				fprintf(stdout, "WARNING: Address is outside of file: %.2X %.2X ", currInfo.firstByte, currInfo.secondByte );
				fprintf(pPointerLogFile, "WARNING: Address is outside of file ");
			}

			fprintf(pPointerLogFile, "%.2X %.2X                   %.2X %.2X			%.2X %.2X (@%.4X)	 			",
				currInfo.secondByte, currInfo.firstByte, currInfo.firstByte, currInfo.secondByte, currInfo.newFirstByte, currInfo.newSecondByte, currInfo.newLoc);

			int byteIndex = 0;
			while(currInfo.pointerStart != currInfo.pointer)
			{
				fprintf(pPointerLogFile, "%.2X ", currInfo.origBytes[byteIndex++]);
				++currInfo.pointerStart;
			}
			fprintf(pPointerLogFile, "%.2X %.2X						%s\n", currInfo.secondByte, currInfo.firstByte, currInfo.pointerType.c_str());

			FindPotentialDuplicatePointers(currInfo, fileBytes, logInfo, pPossiblePointersLogFile);
		}
		
		//write out translated file
		for( BytesList::const_iterator newBytesIter = fileBytes.begin(); newBytesIter != fileBytes.end(); ++newBytesIter)
		{
			fputc( (char)*newBytesIter, pOutEveFile);
		}		

		fclose(pInEngFile);
		fclose(pInEveFile);
		fclose(pOutEveFile);
		fclose(pPointerLogFile);
		fclose(pTextInsertLogFile);
	}//for(eve files)
}

void ConvertSaveFromSSFToYabuse()
{
	FILE *pYabauseFile = NULL;
	FILE *pSSFFile = NULL;

	fopen_s(&pSSFFile,		"DF2_DATA_01.bin", "rb");
	fopen_s(&pYabauseFile,	"bkram.bin", "r+b");	

	assert(pSSFFile);
	assert(pYabauseFile);

	/*
	fseek(pYabauseFile, 256, SEEK_SET);

	char yabauseHeader2[32] = { 0xFF, 0x80, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x44, 0xFF, 0x46, 0xFF, 0x32, 0xFF, 0x5F,
								0xFF, 0x44, 0xFF, 0x41, 0xFF, 0x54, 0xFF, 0x41, 0xFF, 0x5F, 0xFF, 0x30, 0xFF, 0x31, 0xFF, 0x00 };
	fwrite(yabauseHeader2, sizeof(yabauseHeader2), 1, pYabauseFile);
	
	*/
	fseek(pYabauseFile, 0x450, SEEK_SET);
	fseek(pSSFFile,		0x52, SEEK_SET);

	int ssfByte = 0;
	int count = 0;
	while( ssfByte != EOF )
	{
		ssfByte = fgetc(pSSFFile);
		if(ssfByte == EOF)
			break;

		++count;
		FPUTC_VERIFIED( 0xFF, pYabauseFile);
		FPUTC_VERIFIED( ssfByte, pYabauseFile);
	}

	fclose(pSSFFile);
	fclose(pYabauseFile);
}

/*////////////////////////////////////////////////
					BIN Files					
From FaustWolf:

Pointers in these files are all four bytes long, and are absolute (this is in contrast to the relative pointers we were working with in the story files). 
A pointer in these files looks like one of the following:

06 06 XX XX
06 07 XX XX

Since the file is loaded to address 0x06060000 in RAM, the pointers work like this:

06 06 XX XX points somewhere in the range 0x0000 ~ 0xFFFF in the file itself.
06 07 XX XX points somewhere in the range 0x10000 ~ 0x1FFFF in the file itself.

I.e., if a pointer reads 06 06 DC FC, it's pointing to the address 0xDCFC within the file. If the pointer reads 06 07 DC FC, it's pointing to address 0x1DCFC in the file.
////////////////////////////////////////////////*/

enum EBinFileType
{
	kBIN_SPE,
	kBIN_TA,
	kBIN_NAI,
	kBIN_COUNT
};

struct BinAddress
{
	unsigned int textStart;
	unsigned int textEnd;
	unsigned int pointerStart;
	unsigned int pointerEnd;
	unsigned int postPointerStart;
};

BinAddress GBinAddresses[kBIN_COUNT] = 
{
//	textStart	textEnd		ptrStart	ptrEnd		postPtr
	{0x4220,	0xB630,		0x19E10,	0x1A884,	0x1A888},	//SPE
	{0x17A9C,	0x19D64,	0x26BF4,	0x270A4,	0x270A4},	//TA
	{0xC548,	0x13888,	0x195AC,	0x19E80,	0x19E84}	//NAI
};

const int binStartPointer		= 0x06;
const int binStartPointer2_a	= 0x06;
const int binStartPointer2_b	= 0x07;

bool Bin_GetNextPointer(FILE *pInFile, unsigned int currFileLoc, unsigned int &outFileLoc, unsigned int &pointer)
{
	if( fseek(pInFile, currFileLoc, SEEK_SET) != FSEEK_SUCCESS )
	{
		return false;
	}

	int currByte = 0;
	bool bFoundPointerStart = false;
	while(currByte != EOF)
	{
		currByte = fgetc(pInFile);
		if( currByte == EOF )
		{
			return false;
		}

		if( bFoundPointerStart )
		{
			if( !(currByte == binStartPointer2_a || currByte == binStartPointer2_b) )
			{
				bFoundPointerStart = false;
				continue;
			}

			outFileLoc = static_cast<unsigned int> ( ftell(pInFile) ) - 2;
			
			return true;
		}

		if( currByte == binStartPointer )
		{
			bFoundPointerStart = true;
			continue;
		}
	}
	return false;
}

bool Bin_GetTextAddress(FILE *pInFile, unsigned int &inOutFileLoc, unsigned int &outTextPointer)
{
	if( fseek(pInFile, inOutFileLoc, SEEK_SET) != FSEEK_SUCCESS )
	{
		return false;
	}

	//Get type (0x0606 or 0607)
	//Big endian byte order for pointers
	unsigned int firstByte	= fgetc(pInFile); if( firstByte == EOF ) return false;
	unsigned int secondByte = fgetc(pInFile); if( secondByte == EOF ) return false;
	unsigned short type		= (firstByte << 8) | (secondByte & 0xff);
	
	//Pointer to text	
	firstByte				= fgetc(pInFile); if( firstByte == EOF ) return false;
	secondByte				= fgetc(pInFile); if( secondByte == EOF ) return false;	
	unsigned short textAddr = (firstByte << 8) | (secondByte & 0xff);

	outTextPointer = static_cast<unsigned int>(textAddr);
	
	if(type == (unsigned short)0x0607)
	{
		outTextPointer += 0x10000;
	}

	inOutFileLoc += 4;


	return true;
}

void Bin_GetTextString(FILE *pInFile, FILE *pOutFile, unsigned int textLoc)
{
	int currByte = 0;

	if( fseek(pInFile, textLoc, SEEK_SET) != FSEEK_SUCCESS )
	{
		return;
	}

	bool bExpectGeneralName = false;
	bool bFoundStart = false;
	bool bZeroFound = false;

	while(currByte != EOF)
	{
		currByte = fgetc(pInFile);
		
		if( currByte == endText1 )
		{
			break;
		}

		if( bZeroFound && currByte == 0x09 )
		{
			break;
		}
		else if( currByte == 0 )
		{
			bZeroFound = true;
		}

		if( bFoundStart && currByte == 0 )
		{
			break;
		}

		//special character like a new line or general's name
		if(currByte < 0x40 || bExpectGeneralName )
		{
			if( currByte > 0x1F )
			{
				bFoundStart = true;
				FPUTC_VERIFIED(currByte, pOutFile);			
			}			
		}
		//two byte kanji character
		else
		{
			bExpectGeneralName	= false;
			bFoundStart			= true;

			//japanese dump
			FPUTC_VERIFIED(currByte, pOutFile); //write first byte
			FGETC_VERIFIED(currByte, pInFile);  //grab second byte
			
			//Account for already translated text
			if( currByte != 0x0D )
			{
				FPUTC_VERIFIED(currByte, pOutFile); //write second byte
			}
		}
	}

	FPUTC_VERIFIED('\n', pOutFile);
}

void DumpBinJapaneseText()
{
	//Grab all .bin flies
	vector<string> binFiles;
	GetFilesInDir(df2BinFilesPath.c_str(), "BIN", binFiles);

	//Go through all bin files
	const string binExtension(".BIN");
	const string japDumpExtension(".txt");
	for(size_t currFile = 0; currFile < binFiles.size(); ++currFile)
	{
		const string binFileName		= df2BinFilesPath	+ binFiles[currFile] + binExtension;
		const string japDumpFileName	= df2BinJapDumpPath + binFiles[currFile] + japDumpExtension;
		FILE *pInFile					= NULL;
		FILE *pOutFile					= NULL;
		
		fopen_s(&pInFile,  binFileName.c_str(), "rb");
		fopen_s(&pOutFile, japDumpFileName.c_str(), "w");
		
		assert(pInFile);
		assert(pOutFile);
		
		EBinFileType binFileType = kBIN_SPE;
		if(binFileName.find("SPE_") != string::npos)
			binFileType = kBIN_SPE;
		else if(binFileName.find("TA_") != string::npos)
			binFileType = kBIN_TA;
		else if(binFileName.find("NAI_") != string::npos)
			binFileType = kBIN_NAI;
		else
		{
			assert(1 == 0);
		}

		unsigned int currPointerLoc = GBinAddresses[binFileType].pointerStart;
		unsigned int textAddress	= 0;
		int lineCount = 0;
		if(pInFile)
		{
			while( 1 )
			{		
				//Get location of the text string
				if( !Bin_GetTextAddress(pInFile, currPointerLoc, textAddress) )
				{
					break;
				}

				if( textAddress <= GBinAddresses[binFileType].textEnd )
				{
					Bin_GetTextString(pInFile, pOutFile, textAddress);					
					++lineCount;
				}
				else
				{
					lineCount = lineCount;
				}

				//Find location in the file where the next pointer starts
				if( !Bin_GetNextPointer(pInFile, currPointerLoc, currPointerLoc, textAddress) )
				{
					break;
				}

				if( currPointerLoc > GBinAddresses[binFileType].pointerEnd )
				{
					break;
				}

				if(lineCount == 570 && binFileType == kBIN_SPE)
				{
					lineCount = 570;
				}
			}
		}

		fclose(pInFile);
		fclose(pOutFile);
	}
}

//-Just write translated text into the file starting at the beginning of the text block.
//-Make sure to pad so that strings begin at multiple of 4
//-Keep track of locations for where strings are being written
//-Write these locations into the pointer table

bool WordWrap( const char *pString, size_t strLen, std::string &outString)
{
   //Insert the translated string
	int stringsPrinted	= 0; //already have an indent
	int totalPrinted	= 0;
	int numLines		= 0;
	int wordStartIndex	= -1;

	char		tmp[1024];
	int			tmpCount = 0;
	const int	maxPrint = 36 + 36 + 35;
	int			maxPrintForLine = 36;

	memset(tmp, 0, sizeof(tmp));
	bool bAutoWrapping = false;
	for(size_t i = 0; i < strLen; ++i)
	{
		if( totalPrinted == maxPrint )
		{			
			outString = tmp;
			return false;
		}

		assert(totalPrinted < maxPrint);

		const char currLetter = pString[i];

		//discard new lines and what not
		if(currLetter < 0x20 || (currLetter == ' ' && (stringsPrinted == 0 && !bAutoWrapping)) )
			continue;

		//insert our new string
		if( !(currLetter == ' ' && (stringsPrinted + 1 > maxPrintForLine)) )
		{
			tmp[tmpCount++] = currLetter;
		
			//save off start of the word
			if(currLetter != ' ')
			{
				if(wordStartIndex == -1)
				{
					wordStartIndex = tmpCount-1; //(int)i
				}
			}
			else
				wordStartIndex = -1;

			totalPrinted++;

			if( totalPrinted == maxPrint )
			{
				outString = tmp;
				return false;
			}
		}
		else
			wordStartIndex = -1;

		bAutoWrapping = false;

		if(++stringsPrinted > maxPrintForLine)
		{
			if(++numLines == 3)
				break;

			if(numLines == 2)
				maxPrintForLine = 35;

			stringsPrinted = 0;

			//If the 37th character is a space, just remove the space and let the game autowrap
			if( wordStartIndex > -1 && tmp[wordStartIndex] == ' ' && (i - wordStartIndex) == 0 )
			{
				bAutoWrapping = true;			
			}
			else
			{
				//insert new line
				if( wordStartIndex > -1 )
				{
					memcpy(tmp + wordStartIndex + 1, tmp + wordStartIndex, maxPrintForLine);
					tmp[wordStartIndex] = 0x0D;
					tmpCount++;
					stringsPrinted += tmpCount - wordStartIndex;//((int)i - wordStartIndex);
					assert(tmpCount - wordStartIndex >= 0);//assert(i - wordStartIndex >= 0);
				}
				else					
				{
					tmp[tmpCount++] = 0x0D;
				}

				totalPrinted++;
			}
		}
	}

	outString = tmp;

	return true;
}

bool SplitStringIntoLines(const char *pString, std::string& outString)
{
	size_t len = strlen(pString);
	if( len < 37 )
	{
		outString = pString;
		return true;
	}

	return WordWrap(pString, len, outString);
}

void CopyTranslatedTextIntoBin(FILE *pInEngFile, FILE *pOutBinFile, FILE *pLogFile, const BinAddress& addresses, std::vector<int> &textAddresses )
{	
	//Buffer into which translated text is read into
	char engStrBuffer[1024];
	char *inEnglishStrBuffer = engStrBuffer;
	bool bOutOfSpace = false;
	int lineCount = 0;

	while( 1 )
	{
		//Read in translated text
		char *retValue = fgets(inEnglishStrBuffer, 1024, pInEngFile);
		if( feof(pInEngFile) )
		{
			break;
		}
		inEnglishStrBuffer = strtok(inEnglishStrBuffer, "\n\r\v");
		int strLen = (int)strlen(inEnglishStrBuffer);

		std::string splitString;
		if( !SplitStringIntoLines(inEnglishStrBuffer, splitString) )
		{
			fprintf(pLogFile, "Text doesn't fit: (%i Characters) %s", strlen(inEnglishStrBuffer), inEnglishStrBuffer);
		}

		//Store location of the text
		int textLocation = ftell(pOutBinFile);
		textAddresses.push_back( textLocation );
		
		if( bOutOfSpace )
		{
			continue;
		}

		//Check for out of space error
		if( textLocation + strLen + 2 > addresses.textEnd )
		{
			fprintf(pLogFile, "Ran out of space for line(%i @ 0x%08x): %s \n", lineCount, textLocation, inEnglishStrBuffer);
			bOutOfSpace = true;
			continue;
		}

		//Write translated text
		fwrite(splitString.c_str(), splitString.length()*sizeof(char), 1, pOutBinFile);
		fputc(0x15, pOutBinFile);
		fputc(0, pOutBinFile);

		//Insert padding
		int nextStringAddress = textLocation + strLen + 2 + 1; //0x15 0x00 ss
		int paddingNeeded = 4 - (nextStringAddress) % 4;
		switch(paddingNeeded)
		{
			case 1:
			{
				putc(0,		pOutBinFile);
			}break;

			case 2:
			{
				putc(0,		pOutBinFile);
				putc(0x09,	pOutBinFile);
			}break;

			case 3:
			{
				putc(0,		pOutBinFile);
				putc(0,		pOutBinFile);
				putc(0x09,	pOutBinFile);
			}break;
		}

		++lineCount;
	}
}

void WriteBinTextPointers(FILE *pOutBinFile, std::vector<int> &textAddresses)
{
	for(size_t i = 0; i < textAddresses.size(); ++i)
	{
		int address = textAddresses[i];
		char byte0	= (address & 0xff0000) > 0 ? 0x07 : 0x06;
		char byte1	= (address >> 8) & 0xff;
		char byte2  = address & 0xff;

		fputc(0x06,  pOutBinFile);
		fputc(byte0, pOutBinFile);
		fputc(byte1, pOutBinFile);
		fputc(byte2, pOutBinFile);
	}
}

void CopyBinBeginning(FILE *pInBinFile, FILE *pOutBinFile, const BinAddress &addresses)
{
	char *pDynamicBuffer = new char[addresses.textStart];
	fread(pDynamicBuffer, sizeof(char)*addresses.textStart, 1, pInBinFile);
	fwrite(pDynamicBuffer, sizeof(char)*addresses.textStart, 1, pOutBinFile);
	delete[] pDynamicBuffer;
}

void CopyBinBetweenTextAndPointers(FILE *pInBinFile, FILE *pOutBinFile, const BinAddress &addresses)
{
	int dataSize			= addresses.pointerStart - addresses.textEnd;
	char *pDynamicBuffer	= new char[dataSize];

	//Go to end of text area
	fseek(pInBinFile,  addresses.textEnd, SEEK_SET);
	fseek(pOutBinFile, addresses.textEnd, SEEK_SET);

	//copy data between text and pointers
	fread(pDynamicBuffer, dataSize, 1, pInBinFile);
	fwrite(pDynamicBuffer, dataSize, 1, pOutBinFile);

	//Free resources
	delete[] pDynamicBuffer;		
}

void CopyBinAfterPointers(FILE *pInBinFile, FILE *pOutBinFile, const BinAddress &addresses)
{
	if( addresses.postPointerStart == 0 )
	{
		return;
	}

	//Figure out file size
	fseek(pInBinFile, 0, SEEK_END);
	int fileSize = ftell(pInBinFile);

	//Allocate buffer
	int dataSize = fileSize - addresses.postPointerStart;
	char *pDynamicBuffer = new char[dataSize];
	
	//Read data
	fseek(pInBinFile, addresses.postPointerStart, SEEK_SET);
	fread(pDynamicBuffer, dataSize, 1, pInBinFile);

	//Write it out
	fseek(pOutBinFile, addresses.postPointerStart, SEEK_SET);
	fwrite(pDynamicBuffer, dataSize, 1, pOutBinFile);

	//Free resources
	delete[] pDynamicBuffer;
}

void InsertEnglishTextIntoBin()
{
	//Grab all .bin flies
	vector<string> binFiles;
	GetFilesInDir(df2BinFilesPath.c_str(), "BIN", binFiles);

	//Go through all bin files
	const string binExtension(".BIN");
	const string engExtension(".txt");

	std::vector<int> textAddresses;
	for(size_t currFile = 0; currFile < binFiles.size(); ++currFile)
	{
		//Open file with translated text, if it doesn't exist, then skip translating the bin file
		const string engFileName		= df3BinEngPath + binFiles[currFile] + engExtension;
		FILE *pInEngFile				= NULL;
		fopen_s(&pInEngFile, engFileName.c_str(), "rb");
		if(!pInEngFile)
		{
			continue;
		}

		fprintf(stdout, "Processing %s\n", binFiles[currFile].c_str());

		const string binFileName					= df2BinFilesPath + binFiles[currFile] + binExtension;
		const string translatedFileName				= df2BinPatchPath + binFiles[currFile] + binExtension;
		const string errorFileName					= df2BinPatchPath + binFiles[currFile] + std::string("_Log") + engExtension;

		FILE *pInBinFile							= NULL;
		FILE *pOutBinFile							= NULL;
		FILE *pLogFile								= NULL;

		fopen_s(&pInBinFile,	binFileName.c_str(), "rb");
		fopen_s(&pOutBinFile,	translatedFileName.c_str(), "wb");
		fopen_s(&pLogFile,		errorFileName.c_str(), "w");
		
		assert(pInBinFile);
		assert(pOutBinFile);

		EBinFileType binFileType = kBIN_SPE;
		if(binFileName.find("SPE_") != string::npos)
			binFileType = kBIN_SPE;
		else if(binFileName.find("TA_") != string::npos)
			binFileType = kBIN_TA;
		else if(binFileName.find("NAI_") != string::npos)
			binFileType = kBIN_NAI;
		else
		{
			assert(1 == 0);
		}

		//Clear old data
		textAddresses.clear();

		//Copy the beginning of the file to the output file
		CopyBinBeginning(pInBinFile, pOutBinFile, GBinAddresses[binFileType]);

		//Copy translated text over
		CopyTranslatedTextIntoBin(pInEngFile, pOutBinFile, pLogFile, GBinAddresses[binFileType], textAddresses);
		
		//Copy stuff between text and pointers
		CopyBinBetweenTextAndPointers(pInBinFile, pOutBinFile, GBinAddresses[binFileType]);

		//Write out addresses of the translated text
		WriteBinTextPointers(pOutBinFile, textAddresses);

		//Copy over data that appears after pointers
		CopyBinAfterPointers(pInBinFile, pOutBinFile, GBinAddresses[binFileType]);

		fclose(pInEngFile);
		fclose(pInBinFile);
		fclose(pOutBinFile);
	}
}

void main()
{
#if _DEBUG
	_CrtSetDbgFlag( _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF );
	//	_CrtSetBreakAlloc(404);
#endif

	//EVE file functions
//	DumpJapaneseText();
//	InsertEnglishText();

	//BIN files (SPE_MAIN, TA_MAIN, NAI_MAIN)
//	DumpBinJapaneseText();
	InsertEnglishTextIntoBin();

	return;
}
