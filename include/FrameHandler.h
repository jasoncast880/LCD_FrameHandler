#pragma once

#include <stdexcept>
#include <cstring>
#include <string>

/*
 * there should be 2 frames MAXIMUM in existence, one being drawn,
 * one being processed.
 * this lib considers the following stages of frame gen:
 *
 * 1) game logic (taking input from user and sharing it to cpu, which
 * in turn will make updates to other peripherals, shared memory)
 *
 * 2) asset gathering (get pixel data; NOTE this library assumes that the data
 * is formatted in a specific pixel format and byte order. )
 *  -> for now the assets are stored in assets lib; 
 *  however, a more robust solution is to gather assets from a file in
 *  storage, ie sdc.
 *
 * 3) rendering 
 * This involves taking assets in context with scene configuration variables 
 * to alter the necessary tiles.
 * This can be done in two ways:
 *      1: Only have one frame, and update certain tiles in coordinate so that
 *      you can minimize the lcd's processor time, limit the size of the lcd,
 *      increase the throughput of other peripherals
 *      2: A fully flushed out double buffering system that allows for dynamic
 *      rendering: while one frame is dedicated to steps 1 - 3, another is
 *      able to focus solely on being drawn via spi (likely longest step)
 *
 * (currently only mode 1 is available, implementation of these rendering
 * modes should be done by the frame class.)
 *
 * note that this library takes the assets into account by
 * reading through the data, ie xxx_tileset.cpp as a 'tileset object'
 * which in turn is broken into multiple tiles which can be iterated on
 * (transformed, masked, LATER aliased) in order to create a flexible and
 * robust screen interface.
 * *Which means* i have to write formatting tools for later users so that
 * their assets can be read by this lib
 * ->ALIASING STUFF plsss write
 *
 * 4) drawing
 * Drawing is done on the driver-side and i've seperated that so that this 
 * library can be platform independent & thread-safe, whatever is needed
 *
 * OVERALL GOAL: 
 *      MAXIMIZE:
 *      - frames
 *      - color/formatting options
 *      - user ability to handle data input, pixel output
 *      MINIMIZE: 
 *      - RAM Usage
 *      REQUIRE:
 *      -thread safety
 *      -debuggability (without printf???)
 */
#define ALPHA_CLR_565 0xF81F

struct Tilemap;
struct Tileset;

struct FrameData { //like a rectangle-portion of the screen;for partial updates
    uint16_t x,y,w,h; //in pixels

    Tilemap* tilemap;
    Tileset* tileset;
};

struct Tile { //implement assuming indexed color
public:
    int tile_len;        
    uint16_t* buf_ptr;

    Tile();
    Tile(int tile_len, uint16_t* buf_ptr);
    Tile& operator=(const Tile& other);
    void changePixel(uint16_t index, uint16_t value); //bot left corner (x,y)
    uint16_t getPixel(uint16_t x,uint16_t y); //return indexed color value
                                             
    ~Tile(); //ok
};

struct Tileset{
private:
    uint16_t* bufPtr; //sole purpose to populate tileArr
public:
    int tile_len;
    uint8_t numTiles;

    Tile* tileArr;

    //default constructor for passing by reference
    Tileset();
    Tileset(int tile_len, uint16_t* bufPtr, uint8_t numTiles);
    Tileset(Tile* tiles, uint8_t numTiles); 

    Tileset(const Tileset& other);
    Tileset& operator=(const Tileset& other);

    //remov
    //virtual void render(uint16_t x, uint16_t y, uint8_t tileNum); 
    Tile* getTileData(uint8_t tileNum);
    void setTileData(uint8_t tileNum, Tile* tile);

    ~Tileset(); //need to fix
};

/* maps to the frame, so it should be (assuming 16 pix tilemaps)
 *  .____.
 *  |    | 240:15 tiles
 *  o____.
 *   320:20 tiles
 *
 *  IF using 8 pix tilemap: 40 tiles by 30 tiles
 *
 *  implement feature:
 *  tilemap has to remember which tiles have been changed, so that it can 
 *  re-render the necessary pixels.
 *
 */ 

struct Tilemap{ 
public:
    uint8_t x, y;
    uint8_t tiles_wide, tiles_high;
    Tileset* tileset;
    uint8_t* mapBuf;

    Tilemap();
    Tilemap(Tileset* tileset, uint8_t* mapBuf,uint8_t tiles_wide, uint8_t tiles_high); //take the whole screen
    FrameData render();
    Tile* getTileData(uint8_t tileNum);
    void alterTile(Tile* tile, Tile* newTile);
    uint8_t hashPos(int x_pix,int y_pix); 
    virtual ~Tilemap();
}; 

struct Base: public Tilemap{ //only make 1
public:
    uint8_t* mapGuide;
    size_t guideLen;//number of elements in mapGuide&mapBuf

    /* idea: make a second tileset for masked sprite data
     * allows me to keep the current base without changing the size of tileset
     * !!members associated below!!
     */
    //

    Base();
    Base(Tileset* tileset, uint8_t* mapBuf); //take the whole screen
    
    FrameData render(); //gimme a linked list of tiles
    void printMapGuide();
    
    uint8_t hashPos(int x_pix,int y_pix); 
    //base will never be destroyed (so far)
};

struct Sprite: public Tilemap{
private:
    void getDims();
    uint8_t sprite_size;
    uint8_t statusFlag = 0x00;

    Tile* spliceX(Tile* tile1, Tile* tile2, uint8_t cutoff); //after splicing, insert into sprite_Tileset
    Tile* spliceY(Tile* tile1, Tile* tile2, uint8_t cutoff);
    Tile* merge(Tile* tile1, Tile* tile2);

    struct hashedPix; 
    void tileset_validator(Tileset*,int w, int h);

    Tilemap* getBaseTilemap();
    Tileset* getFinalTileset();

public:
    Base* base;

    // positional data;
    int x,y;
    //describes the height/width of the spritemap
    uint8_t tiles_wide, tiles_high; 
    uint8_t proc_tiles_wide, proc_tiles_high; 
    Tileset* tileset;
    uint8_t* mapBuf;

    Sprite();
    Sprite(Base* base, int x,int y,uint8_t tiles_wide, uint8_t tiles_high, Tileset* tileset, uint8_t* mapBuf);
    ~Sprite();

    uint8_t hashPos(int x_pix,int y_pix); 
    // members created after mashing 
    Tileset* finishedTiles;

    void render(); 
    //serve as a setter and a processing func same time!!
    void render(uint8_t* spriteMapBuf); 
    void render(uint16_t x, uint16_t y); 
    void render(uint16_t x, uint16_t y,uint8_t* spriteMapBuf); 
};

//these below should go into a menu/ui configuration file, on 
//app level
struct Char_16{ //simplified hash-map structure for storing font data
public:
    char glyph;
    uint8_t tileNum;
    size_t len=16*16;
};

struct Font{
public:
    Tileset* tileset;
    char* charBuf;
    Char_16 fontArr[250]; 

    Font();
    Font(Tileset* tileset, char* charBuf,size_t len);
    void printFont(uint8_t x,uint8_t y,std::string txt);
};

