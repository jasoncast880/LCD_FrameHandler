#include "include/FrameHandler.h"

/*
 * @brief: this class will handle all of the buffer rendering *before* the sprites are added. tilemaps & sets used to reduce wram size.
 *
 * control the frame buffer within wram, and you control the display itself.
 * */

/*
 * GOALS OF THIS HELPER FILE:
 * is it faster to:
 * -update specific tiles vs updating whole frame using dma?
 *
 * is it more mem efficient? (definitely, this is the main advantage of tiles)
 *
 */

Tile::Tile(){
    this->tile_len = 16;
    this->buf_ptr = new uint16_t[tile_len*tile_len];
} 
Tile::Tile(int tile_len, uint16_t* buf_ptr){
    this->tile_len = tile_len;
    this->buf_ptr = new uint16_t[tile_len*tile_len];
    for(int i = 0; i<tile_len*tile_len;i++){
        this->buf_ptr[i] = buf_ptr[i];
    }
}

Tile& Tile::operator=(const Tile& copySource){
    //self-assignment check
    if(this == &copySource) return *this;
        
    delete[] buf_ptr; //free old mem
    tile_len = copySource.tile_len;

    //allocate new mem and cpy data
    buf_ptr = new uint16_t[tile_len*tile_len];
    std::memcpy(buf_ptr,copySource.buf_ptr, tile_len*tile_len);
    return *this;
}

void Tile::changePixel(uint16_t index, uint16_t value){
    this->buf_ptr[index]=value;
}

uint16_t Tile::getPixel(uint16_t x, uint16_t y){
    return buf_ptr[x+(y*tile_len)];
}

Tile::~Tile(){
    printf("tile destructor invoked ");
    delete[] buf_ptr;
}

Tileset::Tileset(){}

//for working w raw assets, stored in formatted style
Tileset::Tileset(int tile_len, uint16_t* bufPtr, uint8_t numTiles) { 
    this->bufPtr = bufPtr;
    this->tile_len = tile_len;
    this->numTiles = numTiles;

    //populate the Tile* tileArr
    this->tileArr = new Tile[numTiles];
    for (int i=0;i<numTiles;i++){
        tileArr[i] = Tile(this->tile_len,this->bufPtr+(i*tile_len*tile_len));
    }
}

//for working with established tile arrays, converting tilesets
Tileset::Tileset(Tile* tiles, uint8_t numTiles) { 
    //bufPtr unused 
    this->bufPtr = NULL;
    this->tile_len = tiles[0].tile_len; 
    this->numTiles = numTiles;

    //deepcopy
    this->tileArr = new Tile[numTiles];
    for (int i=0;i<numTiles;i++){
        tileArr[i] = tiles[i]; 
    }
}


Tileset::Tileset(const Tileset& copySource){
    this->tile_len = copySource.tile_len;
    this->numTiles = copySource.numTiles;

    tileArr = new Tile[numTiles];
    for(int i = 0; i<numTiles; i++){
        tileArr[i] = copySource.tileArr[i];
    }
}

Tileset& Tileset::operator=(const Tileset& copySource){
    if((this!=&copySource) && (copySource.bufPtr!=NULL)){
        if (bufPtr!=NULL) 
            delete[] bufPtr;
        if (tileArr!=NULL) 
            delete[] tileArr;

        //ensure a deep copy by allocating buffer space

        tile_len = copySource.tile_len;
        numTiles = copySource.numTiles;

        tileArr = new Tile[numTiles];
        for(int i = 0; i<numTiles; i++){
            tileArr[i] = copySource.tileArr[i];
        }
    }
   
    return *this;
}

Tileset::~Tileset(){
    printf("tileset destructor invoked\n");
    delete[] tileArr;
}

Tile* Tileset::getTileData(uint8_t tileNum){
    return &tileArr[tileNum]; 
}

void Tileset::setTileData(uint8_t tileNum, Tile* tile){
    tileArr[tileNum] = *tile; //trigger Tile assignment op??
}


Tilemap::Tilemap(){
    printf("Tilemap def constructor invoked");
} //default constructor (not used)

Tilemap::Tilemap(Tileset* tileset, uint8_t* mapBuf, uint8_t tiles_wide, uint8_t tiles_high){
    this->tileset = tileset;
    this->mapBuf = mapBuf;
    //HARDCODED SCREEN DIMS, add err handling just in case later.
    this->x=0;
    this->y=0;
    this->tiles_wide = tiles_wide;
    this->tiles_high = tiles_high;
}

FrameData Tilemap::render(){  //asssume this takes over the whole screen

    FrameData frame;
    frame.x = 0;
    frame.y = 0;
    //portablility issue here
    frame.w = 240;
    frame.h = 320;
    //
    frame.tilemap = this;
    frame.tileset = this->tileset;

    //go to other part !!!!
    /*
    int counter = 0;
    for(int i = 0; i<tiles_high; i++){
        for(int j=0;j<tiles_wide; j++){
            //Tile* tile=tileset->getTileData(mapBuf[counter]);
            //draw this tile...
            counter++;
        }
    }
    */
    // !!!!
    // Is it efficient to return a buffer of this size?
    // for a sprite i could see justification, for screen rectification. not for a full 
    // screen update though.

    return frame;
}

Tile* Tilemap::getTileData(uint8_t tileNum){
    printf("Tilemap calls getTileData(%d)\n", tileNum);
    return(tileset->getTileData(mapBuf[tileNum]));
}

Tilemap::~Tilemap(){
    printf("Tilemap Destructor invoked\n");
}

//need to account for edge case rendering
Base::Base(){}
Base::Base(Tileset* tileset, uint8_t* mapBuf){ 
    this->x=0;
    this->y=0;
    this->tiles_wide = 320/tileset->tile_len; //hardcoded screen dims!!
    this->tiles_high = 240/tileset->tile_len;

    this->tileset = tileset;
    this->mapBuf = mapBuf;

    this->guideLen = tiles_wide*tiles_high; //size of arrays
    mapGuide = new uint8_t[guideLen]; //dirty versus. clean tiles guide

    printf("from Base constructor:");
    printf("0x%p\n", &mapGuide[0]);
    /*
    mapGuide[x] = 0: DIRTY : replace tile 'x' with tileset->tiles[mapBuf[x]] 
    mapGuide[x] = 1: CLEAN : dont do nothing
    */
    for(int i=0; i<guideLen; i++){
        mapGuide[i]=1;
    }

}

FrameData Base::render(){
    FrameData frame;

    for(int i = 0; i<guideLen; i++) {
        if(mapGuide[i] == 0) {
            //alter a ds to tell other sw to clean; ie queue,ll
            mapGuide[i]=1;
        } else {
            //dont
        }
    }

    return frame;
}

/*
FrameData Base::render(){

    int counter = 0;
    for(int i=0; i<tiles_high; i++){
        for(int j=0;j<tiles_wide;j++){
            if(*(mapGuidePtr+(tiles_wide*i+j))==0){
                ili9341_writeCommand(NOOP);
            }
            else{ //clean up 'dirty' tiles on a base render pass
                tileset->render((j*tileset->tile_len),
                        (i*tileset->tile_len),
                        mapBuf[counter]);
                *(mapGuidePtr+(tiles_wide*i+j))=0;
            }
            counter++;
        }
    }
    printMapGuide(); //for debug
}
*/

void Base::printMapGuide(){
    //debug function
    printf("Tileguide:\n");
    int j=0;
    for(int i=0; i<guideLen; i++){ 
        printf("%d, ",(*mapGuide+(i)));
        j++;
        if(j==tiles_wide){
            j=0;
            printf("\n");
        }
    }
    printf("EOA\n");
}

Sprite::Sprite(){}

Sprite::Sprite(Base* base, int x, int y, uint8_t tiles_wide, uint8_t tiles_high, Tileset* tileset, uint8_t* mapBuf){

    //assumptions:::::::
    //base, points to tilemap & associated data this sprite is rendered on top of
    this->base = base;

    //positional data
    this->x = x;
    this->y = y;

    //tiles wide, high, dimensions of the rendered sprite
    this->tiles_wide = tiles_wide;
    this->tiles_high = tiles_high;

    this->sprite_size = tiles_wide*tiles_high;

    this->proc_tiles_wide = tiles_wide;
    this->proc_tiles_high = tiles_high;

    //tileset of sprite;; should have the pink alpha filter 
    this->tileset = tileset;

    //tilemap data, can be stored in an array in assets, (may change later)
    this->mapBuf = mapBuf;
    
    /*
    sleep_ms(2000);
    tileset_validator(tileset,tiles_wide, tiles_high);
    printf("inside sprite(..) : sprite tileset ok\n");

    sleep_ms(2000);
    tileset_validator(base->tileset,6,5);
    printf("inside sprite(..) : pointer to base tileset ok\n");
    */

    getBaseTilemap();
    
    getFinalTileset();

    printf("inside constructor ok\n");
}

Tilemap* Sprite::getBaseTilemap(){
    //for x - width
    if( x < tileset->tile_len && x % tileset->tile_len != 0) {
        proc_tiles_wide++;
    } else if ( x > tileset->tile_len && tileset->tile_len % x != 0) {
        proc_tiles_wide++;
    }
    
    //for y - width
    if( y < tileset->tile_len && y % tileset->tile_len != 0) {
        proc_tiles_high++;
    } else if ( y > tileset->tile_len && tileset->tile_len % y != 0) {
        proc_tiles_high++;
    }

    Tile* tiles = new Tile[proc_tiles_wide*proc_tiles_high];

    for(int i = 0;i<proc_tiles_high;i++){
        for(int j = 0;j<proc_tiles_wide;j++){
            Tile* tile = base->getTileData(base->hashPos(
                        (x+j*this->tileset->tile_len),
                        y+i*this->tileset->tile_len));
        }
    }

    Tileset* baseTiles = new Tileset(tiles, sprite_size);
    printf("tileset allocation ok\n");

    printf("deleting tiles tile array:\n");
    delete[] tiles;
    printf("\n");

    Tilemap* baseTilemap = new Tilemap(baseTiles, NULL,proc_tiles_wide,proc_tiles_high);

    return baseTilemap;
    //tileset_validator(baseTiles, tiles_wide, tiles_high); //OK
}

struct Sprite::hashedPix {
    //position in context of a single tile
    int x_pix, y_pix;
};


Tileset* Sprite::getFinalTileset(){

    Tilemap* base_tilemap = getBaseTilemap();

    //extract the pixels from the tileset
    size_t buf_len = tiles_high*tiles_wide*(tileset->tile_len*tileset->tile_len);
    uint16_t* pix_buf = new uint16_t[buf_len];

    // //can move this portion to tileset class later 
    uint16_t counter;
    for( int i = 0 ; i < tiles_high ; i++ ) {
        for(int a = 0; a < tileset->tile_len ; a++ ) {
            for(int j = 0; j < tiles_wide ; j++){
                int curr_idx = hashPos(j*tileset->tile_len,i*tileset->tile_len);
                Tile* tile = tileset->getTileData(curr_idx);
                for( int b ; b < tileset->tile_len ; b++ ) {
                    pix_buf[counter] = tile->getPixel(a,b);
                    counter++;
                }
            }
        }
    }
    // now i have pix_buf

    //now loop through each tile and update base_tiles to filter on all thhe pixels..
    uint16_t x_offset = (x+tileset->tile_len)%tileset->tile_len;
    uint16_t y_offset = (y+tileset->tile_len)%tileset->tile_len;
    //map this onto the base tiles
    for(int i = 0 ; i < buf_len ; i++) {
        if(pix_buf[i]!=ALPHA_CLR_565){
            uint16_t n = base_tilemap->hashPos(i+x_offset,
                    i/((this->tiles_wide)*(tileset->tile_len))); //index in tile arr
            
            int x = (x_offset+i)+tileset->tile_len%tileset->tile_len;
            int y = (i/((this->tiles_wide)*(tileset->tile_len)))+tileset->tile_len%tileset->tile_len;
            uint16_t idx = x+y*tileset->tile_len; //index in the tile's buffer

            base_tilemap->tileset->tileArr[n].changePixel(idx,pix_buf[i]);
        }
    }


    Tileset* final_tileset = base_tilemap->tileset;
    return final_tileset;
}

uint8_t Tilemap::hashPos(int x_pix, int y_pix){
    return ((x_pix/tileset->tile_len)+(y_pix/tileset->tile_len)*(this->tiles_wide));
}

Sprite::~Sprite(){
    //delete bufPtr;

    for(int i=0;i<base->tiles_high;i++){
        for(int j=0;j<base->tiles_wide;j++){
            base->mapGuide[i*tiles_wide+j]=0; //reset the things on base guide
        }
    }
} 

//try: frame data renders, sends its tiles to base. base uses linked list to update clean to dirty, update sprited tiles to dirty.
//
void Sprite::render() { //only relevant subsequent to instantiation; 
                             //on pos/tiles update use the other 3

    this->finishedTiles = getFinalTileset();
}

void Sprite::render(uint8_t* spriteMapBuf){ 
    this->mapBuf = spriteMapBuf;
    delete finishedTiles;

    this->finishedTiles = getFinalTileset(); //revise this method
}

void Sprite::render(uint16_t x, uint16_t y){
    this->x = x;
    this->y = y;

    this->finishedTiles = getFinalTileset();
}

void Sprite::render(uint16_t x, uint16_t y, uint8_t* spriteMapBuf){ 
    this->mapBuf = spriteMapBuf;
    this->x = x;
    this->y = y;

    this->finishedTiles = getFinalTileset();
}

/*
Font::Font(){}
//only works for 16 pixel fonts!!
//Font: Char_16 fontArr[100]
Font::Font(Tileset* tileset, char* charBuf, size_t len){
    //printf("size of struct Char_16: %zu bytes\n",sizeof(struct Char_16));
    this->tileset = tileset;
    for(int i=0;i<(int)len;i++){
        size_t index = static_cast<size_t>(*charBuf);
        fontArr[index].glyph=*charBuf;
        fontArr[index].tileNum=(222-i); //specific to rook tileset
        charBuf++;
    } //hashes all of the chars.
    //printf("finished loop");
}

void Font::printFont(uint8_t x, uint8_t y, std::string txt){
    for(int i=0; i<txt.size(); i++){
        uint8_t temp=fontArr[static_cast<size_t>(txt[i])].tileNum;

        tileset->render(x+(16*i)+2, y, temp);
        ili9341_writeCommand(NOOP);
        sleep_ms(250);
    }
}
*/
