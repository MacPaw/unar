/*
 * XADArParser.m
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
#import "XADArParser.h"

@implementation XADArParser

+(int)requiredHeaderSize { return 6; }

+(BOOL)recognizeFileWithHandle:(CSHandle *)handle firstBytes:(NSData *)data name:(NSString *)name
{
	const uint8_t *bytes=[data bytes];
	int length=[data length];

	if(length<8) return NO;

	return memcmp(bytes,"!<arch>\n",8)==0;
}

static uint64_t ParseDecimal(const uint8_t *ptr,int maxlen)
{
	uint64_t val=0;
	for(int i=0;i<maxlen;i++)
	{
		if(*ptr<'0'||*ptr>'9') break;
		val=val*10+*ptr-'0';
		ptr++;
	}
	return val;
}

static uint64_t ParseOctal(const uint8_t *ptr,int maxlen)
{
	uint64_t val=0;
	for(int i=0;i<maxlen;i++)
	{
		if(*ptr<'0'||*ptr>'7') break;
		val=val*8+*ptr-'0';
		ptr++;
	}
	return val;
}

-(void)parse
{
	CSHandle *fh=[self handle];

	[fh skipBytes:8];

	NSData *filenametable=nil; // TODO: Maybe this shouldn't be autoreleased.

	while(![fh atEndOfFile] && [self shouldKeepParsing])
	{
		uint8_t header[60];
		[fh readBytes:sizeof(header) toBuffer:header];

		if(header[58]!=0x60||header[59]!=0x0a) [XADException raiseIllegalDataException];

		uint64_t timestamp=ParseDecimal(&header[16],12);
		int owner=(int)ParseDecimal(&header[28],6);
		int group=(int)ParseDecimal(&header[34],6);
		int mode=(int)ParseOctal(&header[40],8);
		uint64_t size=ParseDecimal(&header[48],10);

		XADPath *name;

		if(header[0]=='#'&&header[1]=='1'&&header[2]=='/')
		{
			// BSD long filename.
			int namelen=(int)ParseDecimal(&header[3],12);
			uint8_t namebuf[namelen];
			[fh readBytes:namelen toBuffer:namebuf];
			size-=namelen;

			while(namelen && namebuf[namelen-1]==0) namelen--;

			name=[self XADPathWithBytes:namebuf length:namelen separators:XADNoPathSeparator];
		}
		else if(header[0]=='/'&&header[1]==' ')
		{
			// GNU symbol table, ignore.
			[fh skipBytes:size];
			continue;
		}
		else if(header[0]=='/'&&header[1]=='/'&&header[2]==' ')
		{
			// GNU long filename list.
			filenametable=[fh readDataOfLength:(int)size];
			continue;
		}
		else if(header[0]=='/'&&header[1]>='0'&&header[1]<='9')
		{
			// GNU long filename.
			int nameoffs=(int)ParseDecimal(&header[1],14);

			const uint8_t *tablebytes=[filenametable bytes];
			int tablelength=[filenametable length];

			if(nameoffs>=tablelength) [XADException raiseIllegalDataException];

			int endoffs=nameoffs;
			while(endoffs<tablelength&&tablebytes[endoffs]!='\n'&&
			tablebytes[endoffs]!='/') endoffs++;

			name=[self XADPathWithBytes:&tablebytes[nameoffs] length:endoffs-nameoffs separators:XADNoPathSeparator];
		}
		else
		{
			// Regular entry.
			int namelen=16;
			while(namelen && (header[namelen-1]==' '||header[namelen-1]=='/'))
			namelen--;

			name=[self XADPathWithBytes:header length:namelen separators:XADNoPathSeparator];
		}

		off_t offs=[fh offsetInFile];

		NSMutableDictionary *dict=[NSMutableDictionary dictionaryWithObjectsAndKeys:
			name,XADFileNameKey,
			[NSNumber numberWithUnsignedLongLong:size],XADFileSizeKey,
			[NSNumber numberWithUnsignedLongLong:size],XADCompressedSizeKey,
			[NSNumber numberWithUnsignedLongLong:size],XADDataLengthKey,
			[NSNumber numberWithUnsignedLongLong:offs],XADDataOffsetKey,
			[NSDate dateWithTimeIntervalSince1970:timestamp],XADLastModificationDateKey,
			[NSNumber numberWithInt:owner],XADPosixUserKey,
			[NSNumber numberWithInt:group],XADPosixGroupKey,
			[NSNumber numberWithInt:mode],XADPosixPermissionsKey,
			[self XADStringWithString:@"None"],XADCompressionNameKey,
		nil];

		[self addEntryWithDictionary:dict];

		[fh seekToFileOffset:((offs+size+1)&~1)];
	}
}

-(CSHandle *)handleForEntryWithDictionary:(NSDictionary *)dict wantChecksum:(BOOL)checksum
{
	return [self handleAtDataOffsetForDictionary:dict];
}

-(NSString *)formatName { return @"Ar"; }

@end
