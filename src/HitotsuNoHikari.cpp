/**
* HitotsuNoHikari.cpp
* TEXB Optimizer with Zopfli compression.
**/

#include <iostream>
#include <fstream>
#include <cerrno>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

#include <stdint.h>
#include <zlib.h>
#include <zopfli.h>

#define File_Error(f) {\
	int err=errno;\
	std::cerr << f << ": " << strerror(err) << std::endl << std::endl;\
	return err;\
	}

// We don't need the GZip compression. Just zLib-compatible.
extern "C" void ZopfliGzipCompress(const ZopfliOptions* options,const unsigned char* in, size_t insize,unsigned char** out, size_t* outsize) {}

// Copied from Itsudemo TEXBLoad.cpp line 36 @ 78d1f72f17a22cbcbddb7646a88ad39e77153130
uint8_t GetBytePerPixel(uint16_t TexbFlags)
{
	uint8_t iff=TexbFlags&7;
	switch(uint8_t(TexbFlags)>>6)
	{
		case 0:
		case 1:
		case 2:
			return 2;
		case 3:
		{
			switch(iff)
			{
			case 0:
				return 1;
			default:
				return iff;
			}
		}
		default:
			return 0;
	}
}

inline const char* get_basename(const char* a)
{
	const char* b=strrchr(a,'\\'),* c=strrchr(a,'/');
	return b==c?a:(std::max(b,c)+1);
}

template<typename T> inline T* null_coalescing(T* a,T* b)
{
	return a==NULL?b:a;
}

int main(int argc,char* argv[])
{
	std::cerr << "HitotsuNoHikari. Compress TEXB with Zopfli's DEFLATE compression." << std::endl;
	if(argc<2)
	{
		std::cerr << "Usage: " << get_basename(argv[0]) << "<input texb> [output texb=input texb]" << std::endl;
		std::cerr << "Recompress TEXB with Zopfli's DEFLATE compression.\n" << std::endl;
		return 0;
	}

	FILE* f;
	ZopfliOptions zopt;
	z_stream z_state;
	uint32_t temp_int;
	uint32_t texb_flags_pos;
	uint32_t old_texb_size;
	uint32_t new_texb_size;
	uint16_t texb_flags;
	uint16_t width;
	uint16_t height;
	uint8_t* texb_raw_buffer=NULL;
	uint8_t* temp_dynamic_buffer=NULL;
	uint8_t temp_buffer[16];
	uint8_t bpp;

	if(memcmp(argv[1],"-",2)==0) f=stdin;
	else f=fopen(argv[1],"rb");
	if(f==NULL)
		File_Error(argv[1])
	if(fread(temp_buffer,8,1,f)==0)
		File_Error(argv[1])
	if(memcmp(temp_buffer,"TEXB",4))
	{
		fclose(f);
		std::cerr << argv[1] << ": Not a TEXB file\n" << std::endl;
		return -1;
	}

	old_texb_size=ntohl(*reinterpret_cast<uint32_t*>(temp_buffer+4))+8;
	fread(temp_buffer+8,2,1,f);
	fseek(f,ntohs(*reinterpret_cast<uint16_t*>(temp_buffer+8)),SEEK_CUR);
	fread(temp_buffer+8,2,2,f);
	width=ntohs(*reinterpret_cast<uint16_t*>(temp_buffer+8));
	height=ntohs(*reinterpret_cast<uint16_t*>(temp_buffer+10));
	texb_flags_pos=ftell(f);
	fread(temp_buffer+8,2,1,f);
	texb_flags=ntohs(*reinterpret_cast<uint16_t*>(temp_buffer+8));
	bpp=GetBytePerPixel(texb_flags);
	fseek(f,4,SEEK_CUR);

	fread(temp_buffer+8,2,1,f);
	*reinterpret_cast<uint16_t*>(temp_buffer+8)=ntohs(*reinterpret_cast<uint16_t*>(temp_buffer+8));
	for(uint16_t i=0;i<*reinterpret_cast<uint16_t*>(temp_buffer+8);i++)
	{
		fread(temp_buffer,4,1,f);
		if(memcmp(temp_buffer,"TIMG",4))
		{
			fclose(f);
			std::cerr << argv[1] << ": Invalid TEXB file.\n" << std::endl;
			return -1;
		}

		fread(temp_buffer+10,2,1,f);
		fseek(f,ntohs(*reinterpret_cast<uint16_t*>(temp_buffer+10)),SEEK_CUR);
	}
	old_texb_size-=ftell(f);

	std::cerr << "File: " << argv[1] << std::endl << "TEXB ";
	// Now decompress if it's compressed
	if(texb_flags&8)
	{
		// Compressed
		std::cerr << "compressed ";
		fread(temp_buffer+4,4,1,f);
		old_texb_size-=4;
		if(*reinterpret_cast<uint32_t*>(temp_buffer+4)==0)
		{
			// zLib compressed. Deflate it
			std::cerr << "zlib deflate" << std::endl;
			memset(&z_state,0,sizeof(z_stream));
			if(temp_int=inflateInit(&z_state)!=Z_OK)
			{
				fclose(f);
				std::cerr << argv[1] << ": zLib error: " << zError(temp_int) << std::endl << std::endl;
				return temp_int;
			}
			
			temp_int=width*height*bpp;
			temp_dynamic_buffer=new uint8_t[old_texb_size];
			texb_raw_buffer=new uint8_t[temp_int];
			fread(temp_dynamic_buffer,1,old_texb_size,f);
			std::cerr << "Compressed size: " << old_texb_size << std::endl << "Uncompressed size: " << temp_int << std::endl;

			z_state.avail_in=old_texb_size;
			z_state.next_in=temp_dynamic_buffer;
			z_state.avail_out=temp_int;
			z_state.next_out=texb_raw_buffer;

			temp_int=inflate(&z_state,Z_NO_FLUSH);
			if(temp_int!=Z_OK && temp_int!=Z_STREAM_END)
			{
				fclose(f);
				delete[] temp_dynamic_buffer;
				delete[] texb_raw_buffer;
				std::cerr << argv[1] << ": zLib error: " << zError(temp_int) << std::endl << std::endl;
				return temp_int;
			}

			inflateEnd(&z_state);
			delete[] temp_dynamic_buffer;
		}
		else
		{
			fclose(f);
			std::cerr << "unknown" << std::endl << argv[1] << ": Unknown compression\n" << std::endl;
			return -1;
		}
	}
	else
	{
		std::cerr << "uncompressed" << std::endl;
		temp_int=width*height*bpp;
		texb_raw_buffer=new uint8_t[temp_int];
		fread(texb_raw_buffer,1,temp_int,f);
		std::cerr << "Uncompressed size: " << temp_int << std::endl;
	}

	std::cerr << std::endl;
	size_t outsize=0;
	char* numiter=getenv("HNH_NUMITER");
	ZopfliInitOptions(&zopt);
	zopt.numiterations=1;

	if(numiter!=NULL)
	{
		char* end;
		zopt.numiterations=strtoul(numiter,&end,0);
		if(zopt.numiterations==0 || *end!=0) zopt.numiterations=1;
	}

	ZopfliCompress(&zopt,ZOPFLI_FORMAT_ZLIB,texb_raw_buffer,width*height*bpp,&temp_dynamic_buffer,&outsize);

	if(outsize==old_texb_size)
	{
		fclose(f);
		std::cerr << "No size change detected.\n" << std::endl;
		return 0;
	}
	else if(outsize>old_texb_size)
	{
		fclose(f);
		std::cerr << "Size is larger: " << outsize << " bytes(previously " << old_texb_size << " bytes) (" << (double(outsize)/double(old_texb_size))*100.0 << "%)\n" << std::endl;
		return 0;
	}

	delete[] texb_raw_buffer;
	fseek(f,4,SEEK_SET);
	fread(temp_buffer+4,4,1,f);
	temp_int=ntohl(*reinterpret_cast<uint32_t*>(temp_buffer+4))-old_texb_size-(((texb_flags&8)==0)?0:4);
	texb_raw_buffer=temp_dynamic_buffer;
	temp_dynamic_buffer=new uint8_t[temp_int];
	fread(temp_dynamic_buffer,1,temp_int,f);
	if(f!=stdin)
		fclose(f);

	const char* out_location=null_coalescing(argv[2],argv[1]);
	if(memcmp(out_location,"-",2)==0) f=stdout;
	else f=fopen(out_location,"w+b");
	if(f==NULL)
		File_Error(out_location)
	
	fwrite("TEXB",4,1,f);
	fwrite("\0\0\0",4,1,f);
	fwrite(temp_dynamic_buffer,1,temp_int,f);
	fwrite("\0\0\0",4,1,f);
	fwrite(texb_raw_buffer,1,outsize,f);
	new_texb_size=ftell(f);
	fflush(f);
	fseek(f,4,SEEK_SET);
	temp_int=htonl(new_texb_size-8);
	fwrite(&temp_int,4,1,f);
	
	// Change flags
	fseek(f,texb_flags_pos,SEEK_SET);
	*reinterpret_cast<uint16_t*>(temp_buffer+8)=htons(texb_flags|8);
	fwrite(temp_buffer+8,2,1,f);

	// End. Close file
	if(f!=stdout)
		fclose(f);

	std::cerr << "New size: " << outsize << " bytes (previously " << old_texb_size << " bytes) (" << (double(outsize)/double(old_texb_size))*100.0 << "%)\n" << std::endl;
	
	free(texb_raw_buffer);
	delete[] temp_dynamic_buffer;
	return 0;
}
