/*
 * XADLArcHandles.m
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
#import "XADLArcHandles.h"

@implementation XADLArcLZSHandle

-(id)initWithHandle:(CSHandle *)handle length:(off_t)length
{
	return [super initWithInputBufferForHandle:handle length:length windowSize:2048];
}

-(int)nextLiteralOrOffset:(int *)offset andLength:(int *)length atPosition:(off_t)pos
{
	if(CSInputNextBit(input)) return CSInputNextBitString(input,8);
	else
	{
		*offset=(int)pos-CSInputNextBitString(input,11)-17;
		*length=CSInputNextBitString(input,4)+2; // TODO: 3 or 2?

		return XADLZSSMatch;
	}
}

@end

@implementation XADLArcLZ5Handle

-(id)initWithHandle:(CSHandle *)handle length:(off_t)length
{
	return [super initWithInputBufferForHandle:handle length:length windowSize:4096];
}

-(void)resetLZSSHandle
{
	flagbit=7;

	for(int i=0;i<256;i++) memset(&windowbuffer[i*13+18],i,13);
	for(int i=0;i<256;i++) windowbuffer[256*13+18+i]=i;
	for(int i=0;i<256;i++) windowbuffer[256*13+256+18+i]=255-i;
	memset(&windowbuffer[256*13+512+18],0,128);
	memset(&windowbuffer[256*13+512+128+18],' ',128-18);
}

-(int)nextLiteralOrOffset:(int *)offset andLength:(int *)length atPosition:(off_t)pos
{
	flagbit++;
	if(flagbit>7)
	{
		flagbit=0;
		flags=CSInputNextByte(input);
	}

	int byte=CSInputNextByte(input);

	if(flags&(1<<flagbit)) return byte;
	else
	{
		int byte2=CSInputNextByte(input);

		*offset=(int)pos-byte-((byte2&0xf0)<<4)-18;
		*length=(byte2&0x0f)+3;

		return XADLZSSMatch;
	}
}

@end

