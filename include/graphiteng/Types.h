#define MAKE_TAG(a,b,c,d) ((a << 24UL) + (b << 16UL) + (c << 8UL) + (d))

#ifndef GRTYPES_INCLUDE
#define GRTYPES_INCLUDE

#include <cstddef>


class Position
{
public:

    Position() : x(0), y(0) { }
    Position(const Position &other) {x = other.x, y = other.y;}
    Position(float inx, float iny) { x = inx; y = iny; }
    Position operator + (Position a) { return Position(x + a.x, y + a.y); }

    float x;
    float y;
};

class Rect
{
public :

    Rect() {}
    Rect(Position origin, Position extent) { org = origin; ext = extent; }
    Rect(const Rect &other) {org = other.org; ext = other.ext;}
    Rect widen(Rect other) { return Rect(Position(org.x > other.org.x ? other.org.x : org.x, org.y > other.org.y ? other.org.y : org.y), Position(ext.x > other.ext.x ? ext.x : other.ext.x, ext.y > other.ext.y ? ext.y : other.ext.y)); }
    Rect operator + (const Position &a) const { return Rect(Position(org.x + a.x, org.y + a.y), Position(ext.x + a.x, ext.y + a.y)); }

    Position org;
    Position ext;
};

#endif // GRTYPES_INCLUDE