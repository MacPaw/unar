/*
 * XADStuffItXEnglishHandle.m
 *
 * Copyright (c) 2017-present, MacPaw Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301  USA
 */
#import "XADStuffItXEnglishHandle.h"
#import "CSMemoryHandle.h"
#import "XADPPMdHandles.h"
#import "XADException.h"
#import "CRC.h"

#define NumberOfWords 100366
#define UncompressedSize 881863
#define CompressedSize 325602

extern uint8_t StuffItXEnglishDictionary[];

@implementation XADStuffItXEnglishHandle

-(id)initWithHandle:(CSHandle *)handle length:(off_t)length
{
	return [super initWithInputBufferForHandle:handle length:length];
}

+(const uint8_t **)dictionaryPointers
{
	static const uint8_t **pointers=NULL;
	if(!pointers)
	{
		CSHandle *mem=[CSMemoryHandle memoryHandleForReadingBuffer:StuffItXEnglishDictionary length:CompressedSize];
		CSHandle *ppmd=[[[XADPPMdVariantIHandle alloc] initWithHandle:mem
		length:UncompressedSize maxOrder:16 subAllocSize:16*1024*1024 modelRestorationMethod:0] autorelease];

		NSData *dictionarywords=[ppmd copyDataOfLength:UncompressedSize];

		const uint8_t *dictbytes=[dictionarywords bytes];

		if((XADCalculateCRC(0xffffffff,dictbytes,UncompressedSize,
		XADCRCTable_edb88320)^0xffffffff)!=0xfb1dcfd5) [XADException raiseUnknownException];

		pointers=malloc(sizeof(uint8_t *)*(NumberOfWords+1));
		pointers[0]=dictbytes;

		const uint8_t *ptr=dictbytes;
		for(int i=1;i<=NumberOfWords;i++)
		{
			while(*ptr!=0x0a) ptr++;
			pointers[i]=++ptr;
		}

	}

	return pointers;
}

-(void)resetByteStream
{
	caseflag=YES;
	wordoffs=wordlen=0;

	esccode=CSInputNextByte(input);
	wordcode=CSInputNextByte(input);
	firstcode=CSInputNextByte(input);
	uppercode=CSInputNextByte(input);
}

-(uint8_t)produceByteAtOffset:(off_t)pos
{
	if(wordoffs<wordlen) return wordbuf[wordoffs++];

	if(CSInputAtEOF(input)) CSByteStreamEOF(self);

	int c=CSInputNextByte(input);

	if(c==esccode)
	{
		caseflag=NO;
		return CSInputNextByte(input);
	}
	else if(c==wordcode||c==firstcode||c==uppercode)
	{
		int c2,index=0;
		for(;;)
		{
			if(CSInputAtEOF(input)) { c2=-1; break; }

			c2=CSInputNextByte(input);
			if((c2<'A'||c2>'Z')&&(c2<'a'||c2>'z')) break;

			index*=52;
			if(c2<='Z') index+=c2-'A'+26+1;
			else index+=c2-'a'+1;
		}

		if(index>=NumberOfWords) [XADException raiseIllegalDataException];

		const uint8_t **pointers=[XADStuffItXEnglishHandle dictionaryPointers];

		wordlen=pointers[index+1]-pointers[index]-1;
		memcpy(wordbuf,pointers[index],wordlen);
		wordoffs=0;

		if(c==uppercode)
		{
			for(int i=0;i<wordlen;i++) wordbuf[i]-=32;
		}
		else if(c==firstcode)
		{
			wordbuf[0]-=32;
		}

		if(caseflag)
		{
			if(wordbuf[0]>='A'&&wordbuf[0]<='Z') wordbuf[0]+=32;
			else if(wordbuf[0]>='a'&&wordbuf[0]<='z') wordbuf[0]-=32;
		}

		if(c2==esccode) c2=CSInputNextByte(input);

		if(c2!=-1) wordbuf[wordlen++]=c2;

		if(c2=='.'||c2=='?'||c2=='!') caseflag=YES;
		else caseflag=NO;

		return wordbuf[wordoffs++];
	}
	else
	{
		if(caseflag)
		{
			if(c>='A'&&c<='Z')
			{
				c+=32;
				caseflag=NO;
			}
			else if(c>='a'&&c<='z')
			{
				c-=32;
				caseflag=NO;
			}
			else caseflag=YES; // useless
		}

		if(c=='.'||c=='?'||c=='!') caseflag=YES;
		else if(c!=' '&&c!='\n'&&c!='\r'&&c!='\t') caseflag=NO;

		return c;
	}
}

@end
