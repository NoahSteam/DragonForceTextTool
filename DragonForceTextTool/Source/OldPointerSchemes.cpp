#if 0
bool GetNextPointer(BytesList::iterator &inStream, BytesList::const_iterator &endStream, BytesList::iterator &outPointer, int &inOutCurrByte)
{
#define INCR_STREAM() ++inStream; ++inOutCurrByte;
#define INCR_PEEK() ++peek; ++peekBytes;

	//Pointers come in one of the following formats
	//Type1 = 29 10 00 00 00 PP PP
	//Type2 = 29 10 01 00 00 PP PP
	//Type3 = 29 10 00 00 ?? 00 ?? 00 07 PP PP
	//Type4 = 29 10 00 00 xx 00 PP PP
	//Type5 = 29 10 nn 00 00 xx xx 9A nn 06 PP PP  where nn is between 00 and 09
	//Type6 = 2F 10 00 00 00 xx 00 mm 06 PP PP //where mm is: maybe some number
                                                                
	static bool b = 0;
	while(inStream != endStream)
	{
		//			00 01 02 03 04 05 06 07 08
		//Type6 =	2F 10 00 00 00 xx 00 mm 06 PP PP //where mm is: maybe some number
		if( *inStream == 0x2F)
		{
			BytesList::iterator peek = inStream;
			int peekBytes = 0;

			//00->01
			INCR_PEEK();

			//01
			if( *peek != (char)0x10 )
				goto failType6;
			INCR_PEEK();

			//02
			if(*peek != 0)
				goto failType6;
			INCR_PEEK();

			//03
			if(*peek != 0)
				goto failType6;
			INCR_PEEK();

			//04
			if(*peek != 0)
				goto failType6;
			INCR_PEEK();

			//05
			INCR_PEEK();

			//06
			if(*peek != 0)
				goto failType6;
			INCR_PEEK();

			//07
			if(*peek == (char)0x06)
			{
				INCR_PEEK();
				inStream = peek;
				inOutCurrByte += peekBytes;
				return true;
			}

			//07->08
			INCR_PEEK();

			//08
			if( *peek != (char)0x06 )
				goto failType6;
			//assert( *peek == 0x06 );
			INCR_PEEK();
			
			//09
			inStream = peek;
			inOutCurrByte += peekBytes;
			return true;
failType6:
			{}
		}

		//Not the beginning of a pointer, so just continue
		if( *inStream != (char)0x29 )
		{
			INCR_STREAM();
			continue;
		}
		INCR_STREAM();

		//Second byte is not that of a pointer, so just continue
		if( *inStream != (char)0x10 )
		{
			INCR_STREAM();
			continue;
		}
		INCR_STREAM();

		//		  00 01 02 03 04 05 06 07 08 09
		//Type5 = 29 10 nn 00 00 xx xx 9A nn 06 PP PP  where nn is between 00 and 09
		if( *inStream >= 0x00 && *inStream <= (char)0x09 )
		{
			BytesList::iterator peek = inStream;			
			int peekBytes = 0;

			//02->03
			INCR_PEEK();

			//03
			if( *peek != 0)
				goto failType5;
			INCR_PEEK();

			//04
			if( *peek != 0)
				goto failType5;
			INCR_PEEK();

			//05->06
			INCR_PEEK();

			//06->07
			INCR_PEEK();

			//07
			if( *peek != (char)0x9A)
				goto failType5;
			INCR_PEEK();

			//08
			if( !(*peek >= 0 && *peek <= (char)0x09) )
				goto failType5;
			INCR_PEEK();

			//09
			if( *peek == (char)0x06)
			{
				INCR_PEEK();

				inStream = peek;
				inOutCurrByte += peekBytes;
				return true;
			}
failType5:
			{}
		}

		//If this is a Type2 pointer
		if( *inStream == 0x01)
		{
			INCR_STREAM();
			if(*inStream != 0)
				continue;

			INCR_STREAM();
			if( *inStream != 0)
			{
				INCR_STREAM();
				if(*inStream != 0)
					continue;
			}
			INCR_STREAM();
			
			outPointer = inStream;
			return true;
		}
		INCR_STREAM();

		if( *inStream != 0x00 )
		{
			continue;
		}

		//Look 7 bytes ahead to see if this is a Type3 pointer
		BytesList::iterator peekAheadIterator = inStream;
		char c = *( ++(++(++(++(++peekAheadIterator)))) );
		if( !b && c == (char)0x07 )
		{
			b = true;
			outPointer = ++peekAheadIterator;
			inOutCurrByte += 6;
			return true;
		}

		INCR_STREAM();
		if( *inStream != 0 )
			continue;

		//We got ourselves a Type1 iterator
		outPointer = ++inStream;
		++inOutCurrByte;
		
		return true;
	}

	return false;
}
#endif
