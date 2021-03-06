/*
 *  Uzebox(tm) Tileset Converter
 *  Copyright (C) 2010  Alec Bourque
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * See the wiki for usage: http://uzebox.org/wiki/index.php?title=Gconvert
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <unistd.h>
#include "tinyxml.h"
#include "lodepng.h"
using namespace std;

#define VERSION_MAJ 1
#define VERSION_MIN 4

void parseXml(TiXmlDocument* doc);
bool process();
unsigned char* loadRawImage();
unsigned char* loadPngImage();
unsigned char* loadImage();
const char* toUpperCase(const char *src);
//void exportType8bpp(unsigned char* buffer, vector<unsigned char*> uniqueTiles, FILE *tf);

struct TileMap {
	const char* varName;
	int left;
	int top;
	int width;
	int height;
};

struct ConvertionDefinition {
	int version;		//"1"
	const char* xformFile;
	const char* inputFile;
	const char* inputType;		//"raw" and "png" are only valid values
	const char* outputType;		//"8bpp" (default) or "code"
	const char* outputFile;
	const char* tilesVarName;
	int backgroundColor;		//optional, specify the mask color for mode 9

	int width;			//in pixels
	int height;			//in pixels
	int tileWidth;		//in pixels
	int tileHeight;		//in pixels

	int mapsPointersSize;
	TileMap* maps;
	int mapsCount;
};

ConvertionDefinition xform;

int main(int argc, char *argv[]) {

	if(argc==1){
		printf("Error: No input file provided.\n\n");
		printf("Uzebox graphics converter version %i.%i.\n",VERSION_MAJ,VERSION_MIN);
		printf("Usage: gconv <configuration.xml>");
		exit( 1 );
	}

    printf("\n*** Gconvert version %i.%i ***\n",VERSION_MAJ,VERSION_MIN);

	char *path=NULL;
	size_t size=0;
	path=getcwd(path,size);
	printf("Current working directory: %s\n",path);

	//load the xform definition file
	printf("Loading transformation file: %s ...\n",argv[1]);
	TiXmlDocument doc (argv[1]);
	xform.xformFile=argv[1];

	doc.LoadFile();
	if ( doc.Error() )
	{
		printf( "Error in %s: %s\n", doc.Value(), doc.ErrorDesc() );
		exit( 1 );
	}

	//parse configuration
	parseXml(&doc);

	//generate include file
	if(!process()){
		exit(1);
	}



	return 0;
}


bool process(){

	unsigned char* buffer=loadImage();

	if(buffer==NULL){
		return false;
	}

	//some validation
    if(xform.maps!=NULL && xform.mapsPointersSize!=8 && xform.mapsPointersSize!=16){
		printf("Error: Invalid map pointers size: %i. Valid values are 8 and 16.\n", xform.mapsPointersSize);
		return false;
    }
    if(xform.width==0 || xform.height==0){
		printf("Error: Invalid image size(%i,%i)\n", xform.width, xform.height);
		return false;
    }
    if(xform.tileWidth==0 || xform.tileHeight==0){
		printf("Error: Invalid tile size(%i,%i)\n", xform.tileWidth, xform.tileHeight);
		return false;
    }

    if((xform.width%xform.tileWidth!=0)){
    	printf("Error: Image width must an integer multiple of the tile width.\n");
    	return false;
    }

    if((xform.height%xform.tileHeight!=0)){
    	printf("Error: Image height must be an integer multiple of the tile height.\n");
    	return false;
    }


	printf("File version: %i\n",xform.version);
	printf("Input file: %s\n",xform.inputFile);
	printf("Input file type: %s\n",xform.inputType);
	printf("Input width: %ipx\n",xform.width);
	printf("Input height: %ipx\n",xform.height);
	printf("Tile width: %ipx\n",xform.tileWidth);
	printf("Tile height: %ipx\n",xform.tileHeight);
	printf("Output file: %s\n",xform.outputFile);
	printf("Output type: %s\n",xform.outputType);
	printf("Tiles variable name: %s\n",xform.tilesVarName);
	if(xform.maps!=NULL){
		printf("Maps pointers size: %i\n",xform.mapsPointersSize);
		printf("Map elements: %i\n",xform.mapsCount);
	}

	int horizontalTiles=xform.width/xform.tileWidth;
	int verticalTiles=xform.height/xform.tileHeight;
	int totalSize=0;

	vector<unsigned char*> uniqueTiles;
	int imageTiles[horizontalTiles*verticalTiles];
	int count=0;

	//build tile file from unique tiles
    FILE *tf = fopen(xform.outputFile,"wt");
    if (!tf){
    	printf("Error: Unable to write to tiles output file %s\n", xform.outputFile);
    	return false;
    }

    fprintf(tf,"/*\n");
    fprintf(tf," * Transformation file: %s\n",xform.xformFile);
    fprintf(tf," * Source image: %s\n",xform.inputFile);
    fprintf(tf," * Tile width: %ipx\n",xform.tileWidth);
    fprintf(tf," * Tile height: %ipx\n",xform.tileHeight);
    fprintf(tf," * Output format: %s\n",xform.outputType);
    fprintf(tf," */\n");


    //build unique tileset
	for(int v=0; v<verticalTiles; v++){
		for(int h=0; h<horizontalTiles; h++){

			unsigned char* tile=new unsigned char[xform.tileWidth*xform.tileHeight];
			int tileIndex=0;
			for(int th=0;th<xform.tileHeight;th++){
				for(int tw=0;tw<xform.tileWidth;tw++){

					int index=(v*horizontalTiles*xform.tileWidth*xform.tileHeight)
							+(h*xform.tileWidth)+(th*horizontalTiles*xform.tileWidth)+ tw;

					tile[tileIndex++]=buffer[index];
				}
			}

			int refIndex=-1;
			//check if tile already exist
			for(int i=uniqueTiles.size()-1;i>=0;i--){
				unsigned char* b=uniqueTiles.at(i);

				int j;
				for(j=0;j<xform.tileWidth*xform.tileHeight;j++){
					if(*b!=tile[j]) break;
					b++;
				}

				if(j==(xform.tileWidth*xform.tileHeight)){
					refIndex=i;	//tile already exist in unique list
					break;
				}
			}

			if(refIndex==-1){

				uniqueTiles.push_back(tile);
				imageTiles[count]=uniqueTiles.size()-1;

				bool allZero=true;
				for(int i=0;i<(xform.tileWidth*xform.tileHeight);i++){
						if(tile[i]!=0)allZero=false;
				}
				if(allZero){
					printf("Blank Tile index: %i\n",(uniqueTiles.size()-1));
				}

			}else{
					imageTiles[count]=refIndex;
			}

			count++;
		}
	}

	//Export maps first
	if(xform.maps!=NULL){

		int index=0;

		for(int m=0; m<xform.mapsCount; m++){
			TileMap map=xform.maps[m];

			//validate map
			if(map.left>horizontalTiles || map.left+map.width>horizontalTiles || map.top>verticalTiles || map.top+map.height >verticalTiles){
				printf("Error: Positions or sizes are out of bound for map: %s\n",map.varName);
				return false;
			}

			fprintf(tf,"#define %s_WIDTH %i\n",toUpperCase(map.varName),map.width);
			fprintf(tf,"#define %s_HEIGHT %i\n",toUpperCase(map.varName),map.height);

			if(xform.mapsPointersSize==8){
				fprintf(tf,"const char %s[] PROGMEM ={\n",map.varName);
			}else{
				fprintf(tf,"const int %s[] PROGMEM ={\n",map.varName);
			}

			fprintf(tf,"%i,",map.width);
			fprintf(tf,"%i",map.height);


			int c=0;
			for(int y=map.top;y<(map.top+map.height);y++){
				for(int x=map.left;x<(map.left+map.width);x++){

					if(c%20==0)	fprintf(tf,"\n"); //wrap line

					fprintf(tf,",");
					index=(y*horizontalTiles)+x;

					if(imageTiles[index]>0xff && xform.mapsPointersSize==8){
						printf("Error: Tile index %i greater than 255.",index);
						return false;
					}

					fprintf(tf,"0x%x",imageTiles[index]);

					c++;

				}
			}

			fprintf(tf,"};\n\n");

			totalSize+=((map.height*map.width)+2)*(xform.mapsPointersSize/8);
		}
	}

	if(xform.outputType==NULL || strcmp(xform.outputType,"8bpp")==0){

		/*Export tileset in 8 bits per pixel format*/
	    fprintf(tf,"#define %s_SIZE %i\n",toUpperCase(xform.tilesVarName),uniqueTiles.size());
	    fprintf(tf,"const char %s[] PROGMEM={\n",xform.tilesVarName);

		int c=0,t=0;
		vector<unsigned char*>::iterator it;
		for(it=uniqueTiles.begin();it < uniqueTiles.end();it++){

			unsigned char* tile=*it;

			for(int index=0;index<(xform.tileWidth*xform.tileHeight);index++){
				if(c>0)fprintf(tf,",");
				fprintf(tf," 0x%x",tile[index]);
				c++;
			}
			fprintf(tf,"\t\t //tile:%i\n",t);
			t++;
		}
		fprintf(tf,"};\n");
		totalSize+=(uniqueTiles.size()*xform.tileHeight*xform.tileHeight);

	}else if(strcmp(xform.outputType,"1bpp")==0){

		/*Export tileset in 1 bits per pixel format*/
	    fprintf(tf,"#define %s_SIZE %i\n",toUpperCase(xform.tilesVarName),uniqueTiles.size());
	    fprintf(tf,"const char %s[] PROGMEM={\n",xform.tilesVarName);

		int c=0,t=0;
		unsigned char b;
		vector<unsigned char*>::iterator it;
		for(it=uniqueTiles.begin();it < uniqueTiles.end();it++){

			unsigned char* tile=*it;

			for(int y=0;y<xform.tileHeight;y++){
				if(c>0)fprintf(tf,",");

				b=0;
				//pack 8 pixels in one byte
				for(int x=0;x<xform.tileWidth;x++){
					if(tile[y*xform.tileWidth+x]!=0) b|=(0x80>>x);
				}
				fprintf(tf," 0x%x",b);
				c++;
			}
			fprintf(tf,"\t\t //tile:%i\n",t);
			t++;
		}
		fprintf(tf,"};\n");
		totalSize+=(uniqueTiles.size()*xform.tileHeight*xform.tileHeight);

	}else if(xform.outputType!=NULL && (strcmp(xform.outputType,"code")==0 || strcmp(xform.outputType,"code60")==0)){

		/*export "code tiles"*/
		fprintf(tf,"#if !(VIDEO_MODE==9 && RESOLUTION==60) \r\n#error The included code-tiles data is only compatible with video mode 9 with 60 columns.\r\n#endif\r\n");
	    fprintf(tf,"#define %s_SIZE %i\n",toUpperCase(xform.tilesVarName),uniqueTiles.size());
	    fprintf(tf,"const char %s[] PROGMEM __attribute__ ((aligned (4))) ={\n",xform.tilesVarName);

		int c=0,t=0;
		vector<unsigned char*>::iterator it;
		for(it=uniqueTiles.begin();it < uniqueTiles.end();it++){

			unsigned char* tile=*it;
			int pos;

			for(int index=0;index<xform.tileHeight;index++){
				if(c>0)fprintf(tf,",");
				pos=xform.tileWidth*index;

				//unsigned char col=tile[pos];

				//pixel 0
				if(xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]){
					fprintf(tf,"0x02,0x2D,"); 										//08 b9       	mov r16,r2
				}else{
					fprintf(tf,"0x%x,0x%x,",tile[pos]&0xf,0xe0|(tile[pos]>>4)); 	//01 e0       	ldi	r16, pixel color	; 1
				}
				fprintf(tf,"0x08,0xb9,"); 											//08 b9       	out	0x08, r16
				fprintf(tf,"0x19,0x91,"); 											//19 91       	ld	r17, Y+
				pos++;

				//pixel 1
				if(xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]){
					fprintf(tf,"0x02,0x2D,"); 										//08 b9       	mov r16,r2
				}else{
					fprintf(tf,"0x%x,0x%x,",tile[pos]&0xf,0xe0|(tile[pos]>>4)); 	//01 e0       	ldi	r16, pixel color	; 1
				}
				fprintf(tf,"0x08,0xb9,"); 											//08 b9       	out	0x08, r16
				fprintf(tf,"0x15,0x9f,");									 		//15 9f       	mul	r17, r21
				pos++;

				//pixel 2
				if(xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]){
					fprintf(tf,"0x02,0x2D,"); 										//08 b9       	mov r16,r2
				}else{
					fprintf(tf,"0x%x,0x%x,",tile[pos]&0xf,0xe0|(tile[pos]>>4)); 	//01 e0       	ldi	r16, pixel color	; 1
				}
				fprintf(tf,"0x08,0xb9,"); 											//08 b9       	out	0x08, r16
				fprintf(tf,"0x08,0x0e,"); 											//08 0e       	add	r0, r24
				fprintf(tf,"0x19,0x1e,"); 											//19 1e       	adc	r1, r25
				pos++;

				//pixel 3
				if(xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]){
					fprintf(tf,"0x02,0x2D,"); 										//08 b9       	mov r16,r2
				}else{
					fprintf(tf,"0x%x,0x%x,",tile[pos]&0xf,0xe0|(tile[pos]>>4)); 	//01 e0       	ldi	r16, pixel color	; 1
				}
				fprintf(tf,"0x08,0xb9,"); 											//08 b9       	out	0x08, r16
				fprintf(tf,"0xf9,0x01,"); 											//f9 01       	movw r30, r18
				fprintf(tf,"0x4a,0x95,"); 											//4a 95       	dec	r20
				pos++;

				//pixel 4
				if(xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]){
					fprintf(tf,"0x02,0x2D,"); 										//08 b9       	mov r16,r2
				}else{
					fprintf(tf,"0x%x,0x%x,",tile[pos]&0xf,0xe0|(tile[pos]>>4)); 	//01 e0       	ldi	r16, pixel color	; 1
				}
				fprintf(tf,"0x08,0xb9,"); 											//08 b9       	out	0x08, r16
				fprintf(tf,"0x09,0xf0,");											//09 f0       	breq	.+2
				fprintf(tf,"0xf0,0x01,"); 											//f0 01       	movw	r30, r0
				pos++;

				//pixel 5
				if(xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]){
					fprintf(tf,"0x02,0x2D,"); 										//08 b9       	mov r16,r2
				}else{
					fprintf(tf,"0x%x,0x%x,",tile[pos]&0xf,0xe0|(tile[pos]>>4)); 	//01 e0       	ldi	r16, pixel color	; 1
				}
				fprintf(tf,"0x08,0xb9,"); 											//08 b9       	out	0x08, r16
				fprintf(tf,"0x09,0x94 "); 											//09 94       	ijmp

				c++;
			}
			fprintf(tf,"\t\t //tile:%i\n",t);
			t++;
		}
		fprintf(tf,"};\n");
		totalSize+=(uniqueTiles.size()*xform.tileHeight*21*2);
	
	}else if(xform.outputType!=NULL && strcmp(xform.outputType,"code80")==0){

		/*export "code tiles"*/
		fprintf(tf,"#if !(VIDEO_MODE==9 && RESOLUTION==80) \r\n#error The included code-tiles data is only compatible with video mode 9 with 80 columns.\r\n#endif\r\n");
	    fprintf(tf,"#define %s_SIZE %i\n",toUpperCase(xform.tilesVarName),uniqueTiles.size());
	    fprintf(tf,"const char %s[] PROGMEM __attribute__ ((aligned (2))) ={\n",xform.tilesVarName);

		int c=0,t=0;
		vector<unsigned char*>::iterator it;
		for(it=uniqueTiles.begin();it < uniqueTiles.end();it++){

			unsigned char* tile=*it;
			int pos;

			for(int index=0;index<xform.tileHeight;index++){
				if(c>0)fprintf(tf,",");
				pos=xform.tileWidth*index;

				/*
				Generate assembly code (pixel color control if r2 (bg) or r3 (fg) is assembled):
				
				out 0x08,r2
				ld	r17, Y+

				out 0x08,r3
				mul	r17, r21

				out 0x08,r2
				add	r0, r24
				adc	r1, r25

				out 0x08,r3
				movw r30, r18
				dec	r20

				out 0x08,r2
				breq	.+2
				movw	r30, r0

				out 0x08,r3
				ijmp
				*/
				

				//pixel 0
				if((xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]) || tile[pos]==0){
					fprintf(tf,"0x28,0xb8,"); 										//28 b8			out 0x08,r2
				}else{
					fprintf(tf,"0x38,0xb8,"); 										//38 b8			out 0x08,r3
				}				
				fprintf(tf,"0x19,0x91,"); 											//19 91       	ld	r17, Y+
				pos++;

				//pixel 1
				if((xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]) || tile[pos]==0){
					fprintf(tf,"0x28,0xb8,"); 										//28 b8			out 0x08,r2
				}else{
					fprintf(tf,"0x38,0xb8,"); 										//38 b8			out 0x08,r3
				}	
				fprintf(tf,"0x15,0x9f,");									 		//15 9f       	mul	r17, r21
				pos++;

				//pixel 2
				if((xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]) || tile[pos]==0){
					fprintf(tf,"0x28,0xb8,"); 										//28 b8			out 0x08,r2
				}else{
					fprintf(tf,"0x38,0xb8,"); 										//38 b8			out 0x08,r3
				}	
				fprintf(tf,"0x08,0x0e,"); 											//08 0e       	add	r0, r24
				fprintf(tf,"0x19,0x1e,"); 											//19 1e       	adc	r1, r25
				pos++;

				//pixel 3
				if((xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]) || tile[pos]==0){
					fprintf(tf,"0x28,0xb8,"); 										//28 b8			out 0x08,r2
				}else{
					fprintf(tf,"0x38,0xb8,"); 										//38 b8			out 0x08,r3
				}	
				fprintf(tf,"0xf9,0x01,"); 											//f9 01       	movw r30, r18
				fprintf(tf,"0x4a,0x95,"); 											//4a 95       	dec	r20
				pos++;

				//pixel 4
				if((xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]) || tile[pos]==0){
					fprintf(tf,"0x28,0xb8,"); 										//28 b8			out 0x08,r2
				}else{
					fprintf(tf,"0x38,0xb8,"); 										//38 b8			out 0x08,r3
				}	
				fprintf(tf,"0x09,0xf0,");											//09 f0       	breq	.+2
				fprintf(tf,"0xf0,0x01,"); 											//f0 01       	movw	r30, r0
				pos++;

				//pixel 5
				if((xform.backgroundColor!=-1 && xform.backgroundColor==tile[pos]) || tile[pos]==0){
					fprintf(tf,"0x28,0xb8,"); 										//28 b8			out 0x08,r2
				}else{
					fprintf(tf,"0x38,0xb8,"); 										//38 b8			out 0x08,r3
				}
				fprintf(tf,"0x09,0x94 "); 											//09 94       	ijmp

				c++;
			}
			fprintf(tf,"\t\t //tile:%i\n",t);
			t++;
		}
		fprintf(tf,"};\n");
		totalSize+=(uniqueTiles.size()*xform.tileHeight*21*2);
	}
	fclose(tf);
	free(buffer);
	printf("File exported successfully!\nUnique tiles found: %i\nTotal size (tiles + maps): %i bytes\n",uniqueTiles.size(),totalSize);


	return true;
}


void parseXml(TiXmlDocument* doc){
	//root
	TiXmlElement* root=doc->RootElement();
	root->QueryIntAttribute("version",&xform.version);

	//input
	TiXmlElement* input=root->FirstChildElement("input");
	xform.inputFile=input->Attribute("file");
	input->QueryIntAttribute("width",&xform.width);
	input->QueryIntAttribute("height",&xform.height);
	input->QueryIntAttribute("tile-width",&xform.tileWidth);
	input->QueryIntAttribute("tile-height",&xform.tileHeight);
	xform.inputType=input->Attribute("type");


	//output
	TiXmlElement* output=root->FirstChildElement("output");
	xform.outputFile=output->Attribute("file");
	TiXmlElement* tiles=output->FirstChildElement("tiles");
	xform.tilesVarName=tiles->Attribute("var-name");
    xform.outputType=output->Attribute("type");
	if(output->QueryIntAttribute("background-color",&xform.backgroundColor)==TIXML_NO_ATTRIBUTE){
		xform.backgroundColor=-1;
	}

	//maps
	TiXmlElement* mapsElem=output->FirstChildElement("maps");
	if(mapsElem!=NULL){
		mapsElem->QueryIntAttribute("pointers-size",&xform.mapsPointersSize);

		//count # of map sub-elements
		const TiXmlNode* node;
		int mapCount=0;
		for(node=mapsElem->FirstChild("map");node;node=node->NextSibling("map"))mapCount++;

		TileMap* maps=new TileMap[mapCount];
		xform.mapsCount=mapCount;
		mapCount=0;
		for(node=mapsElem->FirstChild("map");node;node=node->NextSibling("map")){
			maps[mapCount].varName=node->ToElement()->Attribute("var-name");

			node->ToElement()->QueryIntAttribute("top",&maps[mapCount].top);
			node->ToElement()->QueryIntAttribute("left",&maps[mapCount].left);
			node->ToElement()->QueryIntAttribute("width",&maps[mapCount].width);
			node->ToElement()->QueryIntAttribute("height",&maps[mapCount].height);
			mapCount++;
		}
		if(mapCount>0){
			xform.maps=maps;
		}else{
			xform.maps=NULL;
		}
	}else{
		xform.maps=NULL;
	}

}

//load image in a byte arrays
unsigned char* loadRawImage(){
	unsigned int fileSize=xform.width*xform.height;

	unsigned char* buffer=new unsigned char[fileSize];
	FILE* inputFile;
    size_t ret;

    //Load input image to buffer
	inputFile=fopen(xform.inputFile,"rb");
	if (!inputFile)
	{
		printf("Error: Unable to open input file %s\n", xform.inputFile);
		return NULL;
	}

	ret=fread(buffer,1,fileSize,inputFile);
	fclose(inputFile);
	if(ret != fileSize) {
		printf("Error: File size does not match input parameters. Expected=%i, Read=%i.\n", fileSize,ret);
		return NULL;
	}

	return buffer;
}


unsigned char* loadPngImage(){

	  unsigned char* buffer;
	  unsigned char* image;
	  size_t buffersize, imagesize;
	  LodePNG_Decoder decoder;

	  LodePNG_loadFile(&buffer, &buffersize, xform.inputFile); /*load the image file with given filename*/
	  LodePNG_Decoder_init(&decoder);
	  decoder.settings.color_convert=0; //dont't convert to RGBA
	  LodePNG_decode(&decoder, &image, &imagesize, buffer, buffersize); /*decode the png*/

	  if(decoder.error){
		  if(decoder.error==48){
			  printf("Error in decoding PNG: the input data is empty. Maybe a PNG file you tried to load doesn't exist or is in the wrong path.\n");
		  }else{
			  printf("Error in decoding PNG: %d\n", decoder.error);
		  }
		  /*cleanup decoder*/
		  free(buffer);
		  free(image);
		  LodePNG_Decoder_cleanup(&decoder);
		  return NULL;
	  }

	  if( LodePNG_InfoColor_getBpp(&decoder.infoPng.color)!=8){
		  printf("Error: Invalid PNG image type. Must be PNG-8 with a 256 colors palette.\n");
		  /*cleanup decoder*/
		  free(buffer);
		  free(image);
		  LodePNG_Decoder_cleanup(&decoder);
		  return NULL;
	  }

	  xform.width=decoder.infoPng.width;
	  xform.height=decoder.infoPng.height;

	  /*cleanup decoder*/
	  free(buffer);
	  LodePNG_Decoder_cleanup(&decoder);

	return image;
}

unsigned char* loadImage(){
	unsigned char* buffer;

	//load the source image
	if(strcmp(xform.inputType,"raw")==0){
		buffer=loadRawImage();

	}else if(strcmp(xform.inputType,"png")==0){
		//load image and set height and width in xform struct
		buffer=loadPngImage();

	}else{
		printf( "Unsupported input file type '%s'. Valid values: 'raw' and 'png' \n", xform.inputType );
		return false;
	}

	return buffer;
}


const char* toUpperCase(const char *src){
    char* dest=new char[64];
	strcpy(dest,src);

	int i=0;
	while(dest[i]!=0){
		dest[i]=toupper(src[i]);
		i++;
	}
	return dest;
}
