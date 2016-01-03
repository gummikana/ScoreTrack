#include <sstream>
#include <vector>
#include <iostream>
#include <fstream>

namespace types
{
	struct rect
	{
		rect() : x(0),y(0),w(0),h(0) { }
		rect( float x, float y, float w, float h ) : x(x), y(y), w(w), h(h) { }
		float x, y, w, h;
	};
}
#include "utils/array2d/carray2d.h"
#include "utils/math/cvector2.h"
#include "utils/color/ccolor.cpp"

// #define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

template< class T >
T CastFromString( const std::string& str )
{
	T result;
	std::stringstream ss( str );
	ss.operator>>( result );
	return result;
}

struct TempTexture
{
	~TempTexture() 
	{
		delete [] data;
		data = NULL;
	}

	int width, height;
	unsigned char *data;

	int ColorClamp( int i, int size )
	{
		if( i < 0 )  i += size * (int)(((-i) / size) + 1);
		if( i >= size ) i = i % size;
		return i;
	}

	Uint32 GetPixel( int x, int y, bool include_alpha )
	{
		if( data == NULL ) return 0;

		x = ColorClamp( x, width );
		y = ColorClamp( y, height );
		int bpp = 4;
		int co = ( y * width ) * bpp + x * bpp;

		Uint32 result = 
			data[ co ] << 0 |
			data[ co+1 ] << 8 |
			data[ co+2 ] << 16; 

		if( include_alpha ) 
			result |= data[ co+3 ] << 24;

		return result;
	}
};

void LoadImage( const std::string& filename, ceng::CArray2D< Uint32 >& out_array2d )
{
	bool include_alpha = true;
	int bpp;
	TempTexture* surface = new TempTexture;
	surface->data = stbi_load(filename.c_str(), &surface->width, &surface->height, &bpp, 4);
	if( surface->data == NULL ) std::cout << "LoadImage - Couldn't load file: " << filename << std::endl;
	
	out_array2d.Resize( surface->width, surface->height );

	if( surface )
	{
		for( int y = 0; y < surface->height; ++y )
		{
			for( int x = 0; x < surface->width; ++x )
			{
				out_array2d.Rand( x, y ) = surface->GetPixel( x, y, include_alpha );
			}
		}
	}

	delete surface;
}

void SaveImage( const std::string& filename, const ceng::CArray2D< unsigned int >& image_data )
{
	// do the file and save it
	const int w = image_data.GetWidth();
	const int h = image_data.GetHeight();
	
	unsigned char* pixels = NULL;
	pixels = new unsigned char[ 4 * w * h ];	

	ceng::CColorUint8 c;
	for( int y = 0; y < h; ++y )
	{
		for( int x = 0; x < w; ++x )
		{
			c.Set32( image_data.Rand( x, y ) );

			int p = ( 4 * x + 4 * w * y);
			pixels[ p + 0 ] = c.GetR();
			pixels[ p + 1 ] = c.GetG();
			pixels[ p + 2 ] = c.GetB();
			pixels[ p + 3 ] = c.GetA();
		}
	}

	stbi_write_png( filename.c_str(), w, h, 4, pixels, w * 4 );
	// poro::IPlatform::Instance()->GetGraphics()->ImageSave( filename.c_str(), w, h, 4, pixels, w * 4 );

	delete [] pixels;
	pixels = NULL;
}


struct GridParams
{
	int image_w;
	int image_h;
	int n;		// number of elements
	std::string font;
	float font_size;
	Uint32 background_color;
	Uint32 foreground_color;
	int border_size;
};



struct CharQuad
{
	CharQuad() : rect(), offset(), width( 0 ) { }
	CharQuad( const types::rect& rect, const types::vector2& offset, float width ) : rect( rect ), offset( offset ), width( width ) { }

	types::rect			rect;
	types::vector2		offset;
	float				width;
	float				height;
	
};

unsigned char ttf_buffer[1<<25];

const int bitmap_width = 4096;
const int bitmap_height = 20480;

unsigned char bmap[ bitmap_width * bitmap_height ];
std::vector< CharQuad > char_quads;
	
void CreateFont(
	const std::string& ttf_file,
	float size )
{
	memset( bmap, 0, bitmap_width*bitmap_height );
	memset( ttf_buffer, 0, 1 << 25 );
	FILE* fptr = fopen(ttf_file.c_str(), "rb");

	if( fptr == NULL ) 
	{
		std::cout << "Error reading file: " << ttf_file << std::endl;
		return;
	}

	fread(ttf_buffer, 1, 1<<25, fptr);

	stbtt_bakedchar g_cdata[95]; // ASCII 32..126 is 95 glyphs
	stbtt_BakeFontBitmap( ttf_buffer, 0, size, bmap, bitmap_width, bitmap_height, 32, 95, g_cdata);

	int tsize_x = 0;
	int tsize_y = 0;

	if( char_quads.size() < 95 + 32 )
		char_quads.resize( 95 + 32 );

	for( int i = 0; i < 95; ++i ) 
	{
		char_quads[ i + 32 ] = CharQuad(
				types::rect( 
					(float)g_cdata[ i ].x0, 
					(float)g_cdata[ i ].y0,
					(float)(g_cdata[ i ].x1 - g_cdata[ i ].x0),
					(float)(g_cdata[ i ].y1 - g_cdata[ i ].y0) ),
				types::vector2( 
					g_cdata[ i ].xoff, 
					g_cdata[ i ].yoff ),
				g_cdata[ i ].xadvance );
		
		char_quads[ i + 32 ].height = size;

		if( g_cdata[ i ].x1 > tsize_x ) tsize_x = g_cdata[ i ].x1;
		if( g_cdata[ i ].y1 > tsize_y ) tsize_y = g_cdata[ i ].y1;
	}
}

// -- reading CSV files --

void ReadFileToVector( const std::string& filename, std::vector< std::string >& output )
{
	std::ifstream ifile(filename.c_str());

    while( ifile.good() ) 
	{
        std::string line;
        std::getline(ifile, line);
		output.push_back( line );
    }

	ifile.close();
}

std::string RemoveWhitespace( std::string line )
{
	size_t position = line.find_first_not_of(" \t\r\n");
    if( position != 0 ) 
		line.erase( 0,  position );

    position = line.find_last_not_of(" \t\r\n");
    if( position != line.size() - 1 )
		line.erase( position+1 );

	return line;
}


size_t StringFind( const std::string& _what, const std::string& _line, size_t _begin = 0 )
{
	size_t return_value = _line.find( _what, _begin );
	
	if ( return_value == _line.npos )
		return return_value;

	size_t quete_begin = _line.find("\"", _begin );

	if ( quete_begin == _line.npos || quete_begin > return_value )
		return return_value;

	size_t quete_end = _line.find( "\"", quete_begin+1 );
	
	while( quete_begin < return_value )
	{
		if( quete_end > return_value )
		{
			return_value = _line.find( _what, return_value+1 );
			if ( return_value == _line.npos ) 
				return return_value;
		}

		if( quete_end < return_value )
		{
			quete_begin = _line.find( "\"", quete_end+1 );
			quete_end   = _line.find( "\"", quete_begin+1 );
		}
	}

	return return_value;
}

int StringCount( const std::string& what, const std::string& line )
{
	int result = 0;
	size_t pos = StringFind( what, line, 0 );
	while( pos != line.npos )
	{
		result++;
		pos = StringFind( what, line, pos + what.length() );
	}

	return result;
}


std::vector<std::string> StringSplit( const std::string& _separator, std::string _string )
{
    std::vector <std::string> array;
    size_t position = StringFind( _separator, _string );
	
	while ( position != _string.npos )
    {
        if ( position != 0 )
            array.push_back( _string.substr( 0, position ) );

		_string.erase( 0, position + _separator.length() );

        position = StringFind( _separator, _string );
    }

	if ( _string.empty() == false )
        array.push_back( _string );

    return array;
}

void LoadCSVFile( const std::string& csv_file, ceng::CArray2D< std::string >& result )
{
	std::vector< std::string > input;
	ReadFileToVector( csv_file, input );

	if( input.empty() ) 
		return;

	int height = input.size();
	int width = 1;
	for( size_t i = 0; i < input.size(); ++i )
	{
		int count = StringCount( ",", input[i] );
		if( count > width ) 
			width = count;
	}

	width++;	// 1,2 = 1*,

	result.Resize( width, height );

	for( int y = 0; y < (int)input.size(); ++y )
	{
		std::vector< std::string > data = StringSplit( ",", input[y] );
		for( int x = 0; x < (int)data.size(); ++x )
		{
			cassert( result.IsValid( x, y ) );
			result.At( x, y ) = RemoveWhitespace( data[x] );
		}
	}
}


void BlitImage( ceng::CArray2D< Uint32 >& blit_this, ceng::CArray2D< Uint32 >& to_here, int pos_x, int pos_y )
{
	for( int y = 0; y < blit_this.GetHeight(); ++y )
	{
		for( int x = 0; x < blit_this.GetWidth(); ++x )
		{
			if( to_here.IsValid( x + pos_x, y + pos_y ) )
			{
				to_here.At( x + pos_x, y + pos_y ) = blit_this.At( x, y );
			}
		}
	}
}

void BlitImageWithBorder( ceng::CArray2D< Uint32 >& blit_this, ceng::CArray2D< Uint32 >& to_here, int pos_x, int pos_y, int border_x, int border_y )
{
	for( int y = 0; y < blit_this.GetHeight() + 2 * border_y; ++y )
	{
		for( int x = 0; x < blit_this.GetWidth() + 2 * border_x; ++x )
		{
			if( to_here.IsValid( x + pos_x, y + pos_y ) == false )
				continue;

			if( x < border_x || y < border_y || x > border_x + blit_this.GetWidth() || y > border_y + blit_this.GetHeight() )
			{
				to_here.At( x + pos_x, y + pos_y ) = 0xFFe8e8e8;
			}
			else
			{
				to_here.At( x + pos_x, y + pos_y ) = blit_this.At( x - border_x, y - border_y );
			}
		}
	}
}

Uint32 CastToColor( Uint32 c1, Uint32 c2, unsigned char how_much_of_1 )
{
	if( c1 == c2 ) return c1;
	float how_much = (float)(how_much_of_1) / 255.f;

	/*
	unsigned int c = 255 - how_much_of_1;
	types::fcolor color1;
	color1.Set8( c, c, c, 255 );
	return color1.Get32();*/

	types::fcolor color1( c1 );
	types::fcolor color2( c2 );

	types::fcolor result = how_much * color1 + ( 1.f - how_much ) * color2;

	std::swap( result.r, result.a );
	// result.SetR( ( color1.GetR() > color2.GetR() )? color1.GetR() : color2.GetR() );
	return result.Get32();
}

void BlitText( const std::string& text, ceng::CArray2D< Uint32 >& to_here, int center_x, int center_y, Uint32 fcolor )
{
	float width = 0;
	float height = 0;
	for( std::size_t i = 0; i < text.size(); ++i ) 
	{
		char c = text[i];
		if( c > char_quads.size() ) continue;
		width += char_quads[ c ].width;
		height = std::max( char_quads[ c ].rect.h, height );
	}
	
	int pos_x = (int)( center_x - 0.5f * width + 0.5f); 
	int pos_y = (int)( center_y + 0.5f * height + 0.5f ); 
	
	for( std::size_t i = 0; i < text.size(); ++i ) 
	{
		char c = text[i];
		if( c > char_quads.size() ) continue;

		int px =(int)( pos_x + char_quads[c].offset.x );
		int py =(int)( pos_y + char_quads[c].offset.y );

		for( int y = 0; y < char_quads[c].rect.h; ++y )
		{
			for( int x = 0; x < char_quads[c].rect.w; ++x )
			{
				int bitx = x + char_quads[c].rect.x;
				int bity = y + char_quads[c].rect.y;
			
				unsigned char color = bmap[ bitx + bitmap_width * bity ];
				// if( color > 0 )
				{
					to_here.At( px + x, py + y ) = CastToColor( fcolor, to_here.At( px + x, py + y ), color );
				}
			
			}
		}

		pos_x += char_quads[c].width;
	}
}

void DoAGrid( const GridParams& params, const std::string& output_filename )
{
	ceng::CArray2D< Uint32 > image( params.image_w, params.image_h );
	image.SetEverythingTo( params.background_color );
	CreateFont( params.font, params.font_size );

	// SaveImage( output_filename, image );

	float square_size = (float)( params.image_w * 2 + params.image_h * 2 ) / (float)( params.n + 4 );
	ceng::CArray2D< Uint32 > border( (int)(square_size + 0.5f), (int)(square_size + 0.5f) );
	
	for( int y = 0; y < border.GetHeight(); ++y )
	{
		for( int x = 0; x < border.GetWidth(); ++x )
		{
			Uint32 c = params.foreground_color;		
			if( x >= params.border_size && x < border.GetWidth() - params.border_size &&
				y >= params.border_size && y < border.GetHeight() - params.border_size )
			{
				c = params.background_color;
			}
			border.At( x, y ) = c;
		}
	}

	{
		types::ivector2 pos( 0, 0 );
		types::ivector2 vel( 1, 0 );
		int x_width = params.image_w / border.GetWidth();
		int y_height = params.image_h / border.GetHeight();
		int n = x_width;
		for( int i = 0; i < params.n - 1; ++i )
		{
			BlitImage( border, image, pos.x, pos.y );
			std::stringstream ss;
			ss << ( i);
			BlitText( ss.str(), image, pos.x + border.GetWidth() / 2, pos.y + border.GetHeight() / 2, params.foreground_color );

			types::ivector2 actual_vel = types::ivector2( vel.x * border.GetWidth(), vel.y * border.GetHeight() );
			types::ivector2 new_pos = pos + actual_vel;
			n--;
			if( n <= 0 )
			{
			
			/*if( new_pos.x + actual_vel.x > image.GetWidth() + 2 || 
				new_pos.x + actual_vel.x < -200 ||
				new_pos.y + actual_vel.y > image.GetHeight() + 2 || 
				new_pos.y + actual_vel.y < -200 )
			{*/
				// rotate vel
				if( vel.x > 0 ) { vel.Set( 0, 1 ); n = y_height - 1; }
				else if( vel.y > 0 ) { vel.Set( -1, 0 ); n = x_width; }
				else if( vel.x < 0 ) { vel.Set( 0, -1 ); n = y_height; pos.x = 0; }
				else if( vel.y < 0 ) { vel.Set( 1, 0 ); n = x_width; }
				std::cout << "n, vel: " << n << ", " << vel.x << ", " << vel.y << std::endl;
				actual_vel = types::ivector2( vel.x * border.GetWidth(), vel.y * border.GetHeight() );
				new_pos = pos + actual_vel;
			}
			
			pos = new_pos;
		}
	}

	SaveImage( output_filename, image );
}

void PrintAGrid( ceng::CArray2D< std::string > elements, GridParams params, std::string output_filename )
{
	ceng::CArray2D< Uint32 > image( params.image_w, params.image_h );
	image.SetEverythingTo( params.background_color );
	CreateFont( params.font, params.font_size );

	// SaveImage( output_filename, image );

	float square_w = params.image_w / (float)elements.GetWidth();
	float square_h = params.image_h / (float)elements.GetHeight();
	ceng::CArray2D< Uint32 > border( (int)(square_w + 0.5f), (int)(square_h + 0.5f) );
	
	for( int y = 0; y < border.GetHeight(); ++y )
	{
		for( int x = 0; x < border.GetWidth(); ++x )
		{
			Uint32 c = params.foreground_color;		
			if( x >= params.border_size && x < border.GetWidth() - params.border_size &&
				y >= params.border_size && y < border.GetHeight() - params.border_size )
			{
				c = params.background_color;
			}
			border.At( x, y ) = c;
		}
	}

	for( int y = 0; y < elements.GetHeight(); ++y )
	{
		for( int x = 0; x < elements.GetWidth(); ++x )
		{
			types::ivector2 pos( x, y );
			pos.x *= square_w;
			pos.y *= square_h;

			BlitImage( border, image, pos.x, pos.y );
			BlitText( elements.At( x, y ), image, pos.x + border.GetWidth() / 2, pos.y + border.GetHeight() / 2, params.foreground_color );

		}
	}

	SaveImage( output_filename, image );
}

struct GriddifyParams
{
	types::ivector2 pagesize;
	types::ivector2 bordersize;
};

void Griddify( GriddifyParams params, const ceng::CArray2D< std::string >& cvs_file, std::string output_file )
{

	ceng::CArray2D< Uint32 > data( params.pagesize.x, params.pagesize.y );
	data.SetEverythingTo( 0xFFFFFFFF );

	types::ivector2 size( 0, 0 );
	for( int y = 1; y < cvs_file.GetHeight(); ++y )
	{
		int count = CastFromString< int >( cvs_file.At( 0, y ) );
		std::string filename = cvs_file.At( 1, y );
		if( count <= 0 || filename.empty() ) 
			continue;
		
		ceng::CArray2D< Uint32 > image;
		LoadImage( filename, image );
		if( image.GetWidth() > size.x ) 
			size.x = image.GetWidth();
		if( image.GetHeight() > size.y ) 
			size.y = image.GetHeight();
	}

	size += params.bordersize;

	int perpage_w = params.pagesize.x / size.x;
	int perpage_h = params.pagesize.y / size.y;

	if( params.pagesize.x - (perpage_w * size.x) < 200 ) 
		perpage_w--;

	if( params.pagesize.y - (perpage_h * size.y) < 200 ) 
		perpage_h--;
	
	types::ivector2 offset( ( params.pagesize.x - (perpage_w * size.x) ) / 2,  (params.pagesize.y - (perpage_h * size.y) ) / 2 );

	int i = 0;
	int page = 0;
	int perpage = perpage_w * perpage_h;

	for( int y = 1; y < cvs_file.GetHeight(); ++y )
	{
		int count = CastFromString< int >( cvs_file.At( 0, y ) );
		std::string filename = cvs_file.At( 1, y );
		if( count <= 0 || filename.empty() ) 
			continue;
		
		ceng::CArray2D< Uint32 > image;
		LoadImage( filename, image );

		for( int j = 0; j < count; ++j )
		{
			types::ivector2 pos;
			pos.x = i % perpage_w;
			pos.y = i / perpage_w;
			pos.x = pos.x * size.x + offset.x;
			pos.y = pos.y * size.y + offset.y;
			BlitImageWithBorder( image, data, pos.x, pos.y, params.bordersize.x / 2, params.bordersize.y / 2 );
			i++;
			if( i >= perpage )
			{
				std::stringstream ss;
				ss << output_file << page << ".png";
				SaveImage( ss.str(), data );
				// PrintPage()
				page++;
				i = 0;
				data.SetEverythingTo( 0xFFFFFFFF );
			}
		}
	}

	if( i != 0 )
	{
		std::stringstream ss;
		ss << output_file << page << ".png";
		SaveImage( ss.str(), data );
		// PrintPage()
		page++;
		i = 0;
		data.SetEverythingTo( 0xFFFFFFFF );
	}
}

int main(int argc, char *argv[])
{
	for( int i = 0; i < argc; ++i )
	{
		std::cout << i << ": " << argv[i] << std::endl;
	}

	if( argc < 3 )
	{
		std::cout << "needs more params e.g." << std::endl <<
			"griddify tokens.txt output/token_ (2480) (3508)" << std::endl;
		return 0;
	}
	
	GriddifyParams params;
	params.pagesize.Set( 2480, 3508 );
	params.bordersize.Set( 4, 4 );

	ceng::CArray2D< std::string > elements;
	LoadCSVFile( argv[1], elements );

	Griddify( params, elements, argv[2] );
	
	return 0;
}
