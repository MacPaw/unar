/*
 * XADLZHStaticHandle.m
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
#import "XADLZHStaticHandle.h"
#import "XADException.h"

@implementation XADLZHStaticHandle

-(id)initWithHandle:(CSHandle *)handle length:(off_t)length windowBits:(int)bits
{
	if((self=[super initWithInputBufferForHandle:handle length:length windowSize:1<<bits]))
	{
		literalcode=distancecode=nil;
		windowbits=bits;
	}
	return self;
}

-(void)dealloc
{
	[literalcode release];
	[distancecode release];
	[super dealloc];
}

-(void)resetLZSSHandle
{
	blocksize=0;
	blockpos=0;
}

-(int)nextLiteralOrOffset:(int *)offset andLength:(int *)length atPosition:(off_t)pos
{
	if(blockpos>=blocksize)
	{
		blocksize=CSInputNextBitString(input,16);
		blockpos=0;

		[literalcode release];
		[distancecode release];
		literalcode=nil;
		distancecode=nil;

		literalcode=[self allocAndParseLiteralCode];
		distancecode=[self allocAndParseCodeOfWidth:windowbits<15?4:5 specialIndex:-1];
	}

	blockpos++;

	int lit=CSInputNextSymbolUsingCode(input,literalcode);

	if(lit<0x100) return lit;
	else
	{
		*length=lit-0x100+3;

		int bit=CSInputNextSymbolUsingCode(input,distancecode);
		if(bit==0) *offset=1;
		else if(bit==1) *offset=2;
		else *offset=(1<<(bit-1))+CSInputNextBitString(input,bit-1)+1;

		return XADLZSSMatch;
	}
}

-(XADPrefixCode *)allocAndParseCodeOfWidth:(int)bits specialIndex:(int)specialindex
{
	int num=CSInputNextBitString(input,bits);
	if(num==0)
	{
		int val=CSInputNextBitString(input,bits);
		XADPrefixCode *code=[XADPrefixCode new];
		[code addValue:val forCodeWithHighBitFirst:0 length:0];
		return code;
	}
	else
	{
		int codelengths[num];

		int n=0;
		while(n<num)
		{
			int len=CSInputNextBitString(input,3);
			if(len==7) while(CSInputNextBit(input)) len++;

			codelengths[n++]=len;

			if(n==specialindex)
			{
				int zeroes=CSInputNextBitString(input,2);
				for(int i=0;i<zeroes;i++) codelengths[n++]=0;
			}
		}

		return [[XADPrefixCode alloc] initWithLengths:codelengths numberOfSymbols:num maximumLength:16 shortestCodeIsZeros:YES];
	}
}

-(XADPrefixCode *)allocAndParseLiteralCode
{
	XADPrefixCode *metacode=[self allocAndParseCodeOfWidth:5 specialIndex:3];

	int num=CSInputNextBitString(input,9);
	if(num==0)
	{
		[metacode release];

		int val=CSInputNextBitString(input,9);
		XADPrefixCode *code=[XADPrefixCode new];
		[code addValue:val forCodeWithHighBitFirst:0 length:0];
		return code;
	}
	else
	{
		int codelengths[num];

		int n=0;
		while(n<num)
		{
			unsigned int c=CSInputNextSymbolUsingCode(input,metacode);
			if(c<=2)
			{
				int zeros;
				switch(c)
				{
					case 0: zeros=1; break;
					case 1: zeros=CSInputNextBitString(input,4)+3; break;
					case 2: zeros=CSInputNextBitString(input,9)+20; break;
				}
				if(n+zeros>num) [XADException raiseIllegalDataException];
				for(int i=0;i<zeros;i++) codelengths[n++]=0;
			}
			else codelengths[n++]=c-2;
		}

		[metacode release];

		return [[XADPrefixCode alloc] initWithLengths:codelengths numberOfSymbols:num maximumLength:16 shortestCodeIsZeros:YES];
	}
}

@end
