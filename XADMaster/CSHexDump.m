/*
 * CSHexDump.m
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
#import <Foundation/Foundation.h>

static NSString *HexDump(const uint8_t *bytes,size_t len,int cols,int indent);

@implementation NSData (CSHexDump)

-(NSString *)description
{
	return [NSString stringWithFormat:@"<\n%@\n>",HexDump([self bytes],[self length],16,0)];
}

@end

@implementation NSValue (CSHexDump)

/*-(NSString *)description
{
	NSUInteger size;
	NSGetSizeAndAlignment([self objCType],&size,NULL);
	if(size>0x40000) return [NSString stringWithFormat:@"<Very large %@>",[self class]];

	uint8_t buf[size];
	[self getValue:buf];

	return [NSString stringWithFormat:@"<NSValue:\n%@\n>",HexDump(buf,size,16)];
}*/

@end

static NSString *HexDump(const uint8_t *bytes,size_t len,int cols,int indent)
{
	NSMutableString *str=[NSMutableString string];
	int lines=(len+cols-1)/cols;

	for(int i=0;i<lines;i++)
	{
		for(int j=0;j<indent;j++) [str appendString:@" "];

		[str appendFormat:@"%08x   ",i*cols];

		for(int j=0;j<cols;j++)
		{
			int offs=i*cols+j;
			if(offs>=len) [str appendString:@"  "];
			else [str appendFormat:@"%02x",bytes[i*cols+j]];
			if(j%4==3&&j!=cols-1) [str appendString:@" "];
		}
		[str appendString:@"   "];

		for(int j=0;j<cols;j++)
		{
			int offs=i*cols+j;
			if(offs>=len) [str appendString:@" "];
			else
			{
				int c=bytes[i*cols+j];
				if(c<0x20||(c>=0x80&&c<0xa0)) c='.';
				[str appendFormat:@"%C",(unichar)c];
			}
		}

		if(i!=lines-1) [str appendString:@"\n"];
	}
	return str;
}
